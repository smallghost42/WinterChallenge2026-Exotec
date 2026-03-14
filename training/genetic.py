"""
Genetic algorithm framework for evolving bot parameters.

Implements population management, selection, mutation, crossover,
and generational evolution.
"""

import random
from genome import Genome


class GeneticAlgorithm:
    """Manages a population of genomes and applies evolutionary operations."""

    def __init__(self, population_size=20, elite_count=4, mutation_rate=0.3,
                 mutation_strength=0.2, crossover_rate=0.7, tournament_size=3):
        self.population_size = population_size
        self.elite_count = elite_count
        self.mutation_rate = mutation_rate
        self.mutation_strength = mutation_strength
        self.crossover_rate = crossover_rate
        self.tournament_size = tournament_size
        self.population = []
        self.generation = 0
        self.best_ever = None

    def initialize_population(self, seed_genome=None):
        """
        Create the initial population.
        If a seed_genome is provided, include it and mutated variants.
        """
        self.population = []

        if seed_genome is not None:
            # Include the seed genome
            self.population.append(seed_genome.clone())
            # Create mutated variants of the seed
            for _ in range(self.population_size // 3):
                g = seed_genome.clone()
                g.mutate(mutation_rate=0.5, mutation_strength=0.3)
                g.reset_fitness()
                self.population.append(g)

        # Fill the rest with random genomes
        while len(self.population) < self.population_size:
            self.population.append(Genome.random())

        self.generation = 0

    def tournament_select(self):
        """Select a genome using tournament selection."""
        candidates = random.sample(self.population, min(self.tournament_size, len(self.population)))
        return max(candidates, key=lambda g: g.fitness)

    def select_parents(self):
        """Select two parents for reproduction."""
        p1 = self.tournament_select()
        p2 = self.tournament_select()
        # Ensure different parents if possible
        attempts = 0
        while p2 is p1 and attempts < 5:
            p2 = self.tournament_select()
            attempts += 1
        return p1, p2

    def evolve(self):
        """
        Create the next generation from the current population.
        Uses elitism + tournament selection + crossover + mutation.
        """
        # Sort by fitness (descending)
        self.population.sort(key=lambda g: g.fitness, reverse=True)

        # Track best ever
        if self.best_ever is None or self.population[0].fitness > self.best_ever.fitness:
            self.best_ever = self.population[0].clone()

        next_gen = []

        # Elitism: keep top performers unchanged
        for i in range(min(self.elite_count, len(self.population))):
            elite = self.population[i].clone()
            elite.reset_fitness()
            next_gen.append(elite)

        # Fill the rest with offspring
        while len(next_gen) < self.population_size:
            p1, p2 = self.select_parents()

            if random.random() < self.crossover_rate:
                child = Genome.crossover(p1, p2)
            else:
                child = p1.clone()

            child.mutate(self.mutation_rate, self.mutation_strength)
            child.reset_fitness()
            next_gen.append(child)

        self.population = next_gen
        self.generation += 1

    def get_best(self, n=1):
        """Get the top n genomes by fitness."""
        sorted_pop = sorted(self.population, key=lambda g: g.fitness, reverse=True)
        return sorted_pop[:n]

    def get_stats(self):
        """Get population statistics."""
        fitnesses = [g.fitness for g in self.population]
        return {
            "generation": self.generation,
            "best_fitness": max(fitnesses) if fitnesses else 0,
            "avg_fitness": sum(fitnesses) / len(fitnesses) if fitnesses else 0,
            "worst_fitness": min(fitnesses) if fitnesses else 0,
            "best_ever_fitness": self.best_ever.fitness if self.best_ever else 0,
        }

    def print_stats(self):
        """Print current generation statistics."""
        stats = self.get_stats()
        print(f"\n=== Generation {stats['generation']} ===")
        print(f"  Best fitness:      {stats['best_fitness']:.2f}")
        print(f"  Average fitness:   {stats['avg_fitness']:.2f}")
        print(f"  Worst fitness:     {stats['worst_fitness']:.2f}")
        print(f"  Best ever fitness: {stats['best_ever_fitness']:.2f}")

        best = self.get_best(3)
        for i, g in enumerate(best):
            print(f"  Top {i+1}: {g}")
