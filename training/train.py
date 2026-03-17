#!/usr/bin/env python3
"""
Genetic training loop for evolving bot parameters.

This is the main entry point for running the offline genetic training.
It orchestrates population evolution, match evaluation, and result persistence.

Usage:
    python train.py [--generations N] [--population N] [--matches N] [--parallel N]

Example:
    python train.py --generations 50 --population 20 --matches 4 --parallel 4
"""

import argparse
import json
import os
import random
import sys
import tempfile
import time

from genome import Genome
from genetic import GeneticAlgorithm
from match_runner import (
    evaluate_genome,
    get_baseline_opponents,
    BOT_BINARY,
    REPO_ROOT,
)


def parse_args():
    parser = argparse.ArgumentParser(description="Genetic training for bot parameters")
    parser.add_argument("--generations", type=int, default=30,
                        help="Number of generations (default: 30)")
    parser.add_argument("--population", type=int, default=20,
                        help="Population size (default: 20)")
    parser.add_argument("--matches", type=int, default=4,
                        help="Matches per opponent per genome (default: 4)")
    parser.add_argument("--parallel", type=int, default=1,
                        help="Number of parallel match workers (default: 1)")
    parser.add_argument("--elite", type=int, default=4,
                        help="Number of elite genomes preserved each generation (default: 4)")
    parser.add_argument("--mutation-rate", type=float, default=0.3,
                        help="Probability of mutating each parameter (default: 0.3)")
    parser.add_argument("--mutation-strength", type=float, default=0.2,
                        help="Relative mutation magnitude (default: 0.2)")
    parser.add_argument("--seed", type=int, default=None,
                        help="Random seed for reproducibility")
    parser.add_argument("--resume", type=str, default=None,
                        help="Path to a genome JSON to use as seed")
    parser.add_argument("--output", type=str, default=None,
                        help="Output path for best genome (default: config/optimized_params.json)")
    return parser.parse_args()


def build_bot():
    """Compile the C++ bot."""
    print("Building bot...")
    bot_dir = os.path.join(REPO_ROOT, "bot")
    ret = os.system(f"cd {bot_dir} && make clean && make")
    if ret != 0:
        print("ERROR: Failed to build bot. Make sure g++ is installed.")
        sys.exit(1)
    if not os.path.exists(BOT_BINARY):
        print(f"ERROR: Bot binary not found at {BOT_BINARY}")
        sys.exit(1)
    print("Bot built successfully.")


def build_referee():
    """Compile the Java referee/simulator."""
    print("Building referee...")
    # HeadlessRunner is in src/test/java, so we must compile test sources too.
    ret = os.system(f"cd {REPO_ROOT} && mvn test-compile -q -DskipTests")
    if ret != 0:
        print("ERROR: Failed to build referee. Make sure Maven and Java 17 are installed.")
        sys.exit(1)
    print("Referee built successfully.")


def evaluate_population(ga, opponents, matches_per_opponent, parallel, match_seeds):
    """Evaluate all genomes in the population."""
    total = len(ga.population)
    for idx, genome in enumerate(ga.population):
        print(f"  Evaluating genome {idx+1}/{total}...", end=" ", flush=True)
        evaluate_genome(
            genome,
            opponents,
            matches_per_opponent=matches_per_opponent,
            seeds=match_seeds,
            parallel=(parallel > 1),
        )
        print(f"fitness={genome.fitness:.2f} ({genome})")


def save_results(ga, output_path):
    """Save the best genome and training history."""
    best = ga.get_best(1)[0]

    # Save best genome
    best.save(output_path)
    print(f"\nBest genome saved to: {output_path}")
    print(f"Best genome: {best}")
    print(f"Parameters:")
    for k, v in sorted(best.params.items()):
        print(f"  {k}: {v:.4f}")

    # Save best-ever if different
    if ga.best_ever:
        best_ever_path = output_path.replace('.json', '_best_ever.json')
        ga.best_ever.save(best_ever_path)
        print(f"\nBest-ever genome saved to: {best_ever_path}")

    # Save top 5 genomes
    top5_dir = os.path.join(os.path.dirname(output_path), "top_genomes")
    os.makedirs(top5_dir, exist_ok=True)
    for i, g in enumerate(ga.get_best(5)):
        g.save(os.path.join(top5_dir, f"genome_{i+1}.json"))
    print(f"Top 5 genomes saved to: {top5_dir}/")


def main():
    args = parse_args()

    if args.seed is not None:
        random.seed(args.seed)

    output_path = args.output or os.path.join(REPO_ROOT, "config", "optimized_params.json")

    print("=" * 60)
    print("  GENETIC TRAINING SYSTEM")
    print("=" * 60)
    print(f"  Generations:          {args.generations}")
    print(f"  Population size:      {args.population}")
    print(f"  Matches per opponent: {args.matches}")
    print(f"  Parallel workers:     {args.parallel}")
    print(f"  Elite count:          {args.elite}")
    print(f"  Mutation rate:        {args.mutation_rate}")
    print(f"  Mutation strength:    {args.mutation_strength}")
    print(f"  Output:               {output_path}")
    print("=" * 60)

    # Build
    build_bot()
    build_referee()

    # Get opponents
    opponents = get_baseline_opponents()
    print(f"\nOpponents: {[name for name, _ in opponents]}")

    # Generate match seeds for reproducibility across generations.
    # After gen 1, elite_count extra opponents are added; pre-allocate seeds accordingly.
    max_opponents = len(opponents) + args.elite
    match_seeds = [random.randint(1, 2**31) for _ in range(args.matches * max_opponents)]

    # Initialize GA
    ga = GeneticAlgorithm(
        population_size=args.population,
        elite_count=args.elite,
        mutation_rate=args.mutation_rate,
        mutation_strength=args.mutation_strength,
    )

    # Seed population
    seed_genome = None
    if args.resume:
        print(f"\nResuming from: {args.resume}")
        seed_genome = Genome.load(args.resume)
    else:
        seed_genome = Genome.default()

    ga.initialize_population(seed_genome)
    print(f"\nPopulation initialized with {len(ga.population)} genomes.")

    # Training loop
    start_time = time.time()

    for gen in range(args.generations):
        gen_start = time.time()
        print(f"\n{'='*60}")
        print(f"  GENERATION {gen + 1}/{args.generations}")
        print(f"{'='*60}")

        # Evaluate population
        evaluate_population(ga, opponents, args.matches, args.parallel, match_seeds)

        # Print stats
        ga.print_stats()

        # Save intermediate results every 5 generations
        if (gen + 1) % 5 == 0 or gen == args.generations - 1:
            intermediate_path = output_path.replace('.json', f'_gen{gen+1}.json')
            best = ga.get_best(1)[0]
            best.save(intermediate_path)
            print(f"  Intermediate best saved to: {intermediate_path}")

        gen_time = time.time() - gen_start
        total_time = time.time() - start_time
        print(f"  Generation time: {gen_time:.1f}s | Total: {total_time:.1f}s")

        # Evolve to next generation (except last)
        if gen < args.generations - 1:
            # Add elite genomes from this generation as opponents for the next generation.
            # Remove any previous elite opponents, then add fresh ones.
            opponents = [o for o in opponents if not o[0].startswith("Elite_")]
            elite_genomes = ga.get_best(args.elite)
            for i, bg in enumerate(elite_genomes):
                fd, path = tempfile.mkstemp(suffix='.json', prefix='elite_')
                os.close(fd)
                bg.save(path)
                opp_cmd = f"{BOT_BINARY} --config {path}"
                opp_name = f"Elite_Gen{gen+1}_{i}"
                opponents.append((opp_name, opp_cmd))

            ga.evolve()

    # Save final results
    print(f"\n{'='*60}")
    print("  TRAINING COMPLETE")
    print(f"{'='*60}")
    total_time = time.time() - start_time
    print(f"  Total training time: {total_time:.1f}s")
    save_results(ga, output_path)


if __name__ == "__main__":
    main()
