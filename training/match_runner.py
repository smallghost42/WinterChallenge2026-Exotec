"""
Match runner that interfaces with the Java simulation engine.

Runs matches between bots using the headless runner and extracts scores.
Supports parallel execution of multiple matches.
"""

import os
import subprocess
import tempfile
import random
from concurrent.futures import ProcessPoolExecutor, as_completed
from genome import Genome

# Path configuration (relative to repository root)
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BOT_BINARY = os.path.join(REPO_ROOT, "bot", "bot")


def _write_genome_config(genome, filepath):
    """Write genome parameters to a JSON config file."""
    genome.save(filepath)


def _get_classpath():
    """Get the Java classpath for running HeadlessRunner."""
    cp_file = os.path.join(REPO_ROOT, "target", ".training_cp.txt")
    # Build classpath if not cached
    if not os.path.exists(cp_file):
        ret = subprocess.run(
            ["mvn", "dependency:build-classpath", "-q",
             "-DincludeScope=test", f"-Dmdep.outputFile={cp_file}"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
        )
        if ret.returncode != 0:
            return None
    with open(cp_file, 'r') as f:
        deps = f.read().strip()
    classes = os.path.join(REPO_ROOT, "target", "classes")
    test_classes = os.path.join(REPO_ROOT, "target", "test-classes")
    return f"{classes}:{test_classes}:{deps}"


def run_single_match(bot1_cmd, bot2_cmd, seed=None, timeout=120):
    """
    Run a single match between two bots using the Java headless runner.

    Returns:
        tuple: (score1, score2) or (-1, -1) on error.
    """
    classpath = _get_classpath()
    if classpath is None:
        return -1, -1

    java_args = ["java", "-cp", classpath, "HeadlessRunner", bot1_cmd, bot2_cmd]
    if seed is not None:
        java_args.append(str(seed))

    try:
        result = subprocess.run(
            java_args,
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=timeout,
        )

        # Parse output for SCORES line
        for line in result.stdout.strip().split('\n'):
            if line.startswith("SCORES "):
                parts = line.split()
                if len(parts) >= 3:
                    return int(parts[1]), int(parts[2])

        return -1, -1

    except subprocess.TimeoutExpired:
        return -1, -1
    except Exception:
        return -1, -1


def evaluate_genome(genome, opponents, matches_per_opponent=2, seeds=None, parallel=False):
    """
    Evaluate a genome by running matches against a list of opponents.

    Args:
        genome: The Genome to evaluate.
        opponents: List of (name, command) tuples for opponents.
        matches_per_opponent: Number of matches per opponent.
        seeds: Optional list of seeds to use for reproducibility.
        parallel: Whether to run matches in parallel.

    Returns:
        The genome with updated fitness.
    """
    genome.reset_fitness()

    # Write genome config to a temp file
    config_fd, config_path = tempfile.mkstemp(suffix='.json', prefix='genome_')
    os.close(config_fd)
    _write_genome_config(genome, config_path)

    bot_cmd = f"{BOT_BINARY} --config {config_path}"

    match_args = []
    for opp_name, opp_cmd in opponents:
        for i in range(matches_per_opponent):
            seed = seeds[i % len(seeds)] if seeds else random.randint(1, 2**31)
            # Alternate who plays first
            if i % 2 == 0:
                match_args.append((bot_cmd, opp_cmd, seed))
            else:
                match_args.append((opp_cmd, bot_cmd, seed))

    if parallel:
        results = _run_matches_parallel(match_args)
    else:
        results = _run_matches_sequential(match_args)

    invalid_results = 0

    # Record results
    for idx, (s1, s2) in enumerate(results):
        # Match failed (timeout/classpath/runtime error); do not score it as a draw.
        if s1 < 0 or s2 < 0:
            invalid_results += 1
            continue
        match_within_opp = idx % matches_per_opponent
        if match_within_opp % 2 == 0:
            # We were player 1
            genome.record_match(s1, s2)
        else:
            # We were player 2
            genome.record_match(s2, s1)

    if invalid_results:
        print(f"  Warning: {invalid_results}/{len(results)} matches failed during evaluation.")

    # Cleanup
    try:
        os.unlink(config_path)
    except OSError:
        pass

    return genome


def _run_matches_sequential(match_args):
    """Run matches one at a time."""
    results = []
    for bot1_cmd, bot2_cmd, seed in match_args:
        result = run_single_match(bot1_cmd, bot2_cmd, seed)
        results.append(result)
    return results


def _run_match_worker(args):
    """Worker function for parallel match execution."""
    bot1_cmd, bot2_cmd, seed = args
    return run_single_match(bot1_cmd, bot2_cmd, seed)


def _run_matches_parallel(match_args, max_workers=4):
    """Run matches in parallel using process pool."""
    results = [None] * len(match_args)
    with ProcessPoolExecutor(max_workers=max_workers) as executor:
        future_to_idx = {}
        for idx, args in enumerate(match_args):
            future = executor.submit(_run_match_worker, args)
            future_to_idx[future] = idx

        for future in as_completed(future_to_idx):
            idx = future_to_idx[future]
            try:
                results[idx] = future.result()
            except Exception:
                results[idx] = (-1, -1)

    return results


def get_baseline_opponents():
    """Get the list of baseline opponents for evaluation."""
    opponents = []

    # Bot with existing optimized/baseline parameters
    baseline_config = os.path.join(REPO_ROOT, "config", "optimized_params.json")
    if os.path.exists(baseline_config) and os.path.exists(BOT_BINARY):
        opponents.append(("BaselineBot", f"{BOT_BINARY} --config {baseline_config}"))

    return opponents
