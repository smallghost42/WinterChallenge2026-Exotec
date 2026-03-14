"""
Genome definition for the genetic training system.

Each genome represents a set of tunable parameters for the bot.
Parameters are stored as a dictionary mapping parameter names to float values.
"""

import json
import random
import copy

# Parameter definitions: (min, max, default)
PARAM_DEFS = {
    "apple_weight":       (0.0, 10.0, 3.0),
    "apple_dist_decay":   (0.01, 0.5, 0.15),
    "cluster_bonus":      (0.0, 2.0, 0.4),
    "safety_weight":      (0.0, 10.0, 5.0),
    "dead_end_penalty":   (0.0, 20.0, 8.0),
    "fall_penalty":       (0.0, 10.0, 2.0),
    "wall_penalty":       (0.0, 5.0, 1.0),
    "space_weight":       (0.0, 10.0, 2.0),
    "height_weight":      (0.0, 5.0, 0.5),
    "center_weight":      (0.0, 3.0, 0.3),
    "opponent_proximity": (0.0, 10.0, 3.0),
    "aggression":         (0.0, 5.0, 1.0),
    "survival_priority":  (0.0, 10.0, 4.0),
    "length_bonus":       (0.0, 3.0, 0.2),
    "body_block_bonus":   (0.0, 3.0, 0.5),
}


class Genome:
    """Represents a set of bot parameters (a genome for genetic evolution)."""

    def __init__(self, params=None):
        if params is not None:
            self.params = dict(params)
        else:
            self.params = {k: v[2] for k, v in PARAM_DEFS.items()}
        self.fitness = 0.0
        self.wins = 0
        self.losses = 0
        self.draws = 0
        self.matches_played = 0

    @staticmethod
    def random():
        """Create a genome with random parameter values."""
        params = {}
        for name, (lo, hi, _) in PARAM_DEFS.items():
            params[name] = random.uniform(lo, hi)
        return Genome(params)

    @staticmethod
    def default():
        """Create a genome with default parameter values."""
        return Genome()

    def mutate(self, mutation_rate=0.3, mutation_strength=0.2):
        """
        Mutate this genome in-place.
        mutation_rate: probability of mutating each parameter.
        mutation_strength: relative magnitude of mutation.
        """
        for name, (lo, hi, _) in PARAM_DEFS.items():
            if random.random() < mutation_rate:
                range_size = hi - lo
                delta = random.gauss(0, mutation_strength * range_size)
                self.params[name] = max(lo, min(hi, self.params[name] + delta))
        return self

    @staticmethod
    def crossover(parent1, parent2):
        """
        Create a child genome by combining two parents.
        Uses uniform crossover with blending.
        """
        child_params = {}
        for name in PARAM_DEFS:
            if random.random() < 0.5:
                child_params[name] = parent1.params[name]
            else:
                child_params[name] = parent2.params[name]

            # Occasionally blend
            if random.random() < 0.2:
                alpha = random.uniform(0.3, 0.7)
                lo, hi, _ = PARAM_DEFS[name]
                blended = alpha * parent1.params[name] + (1 - alpha) * parent2.params[name]
                child_params[name] = max(lo, min(hi, blended))

        return Genome(child_params)

    def reset_fitness(self):
        """Reset fitness tracking for a new evaluation cycle."""
        self.fitness = 0.0
        self.wins = 0
        self.losses = 0
        self.draws = 0
        self.matches_played = 0

    def record_match(self, my_score, opp_score):
        """Record the result of a match."""
        self.matches_played += 1
        if my_score > opp_score:
            self.wins += 1
            self.fitness += 3.0
        elif my_score == opp_score:
            self.draws += 1
            self.fitness += 1.0
        else:
            self.losses += 1
            # Small bonus for close losses
            if opp_score > 0 and my_score > 0:
                self.fitness += 0.5 * (my_score / opp_score)

        # Bonus for survival (positive score means birds alive)
        if my_score > 0:
            self.fitness += 0.5

    def to_json(self):
        """Serialize genome to JSON string."""
        return json.dumps(self.params, indent=4)

    def save(self, filepath):
        """Save genome parameters to a JSON file."""
        with open(filepath, 'w') as f:
            json.dump(self.params, f, indent=4)

    @staticmethod
    def load(filepath):
        """Load genome parameters from a JSON file."""
        with open(filepath, 'r') as f:
            params = json.load(f)
        return Genome(params)

    def clone(self):
        """Create a deep copy of this genome."""
        g = Genome(copy.deepcopy(self.params))
        g.fitness = self.fitness
        return g

    def __repr__(self):
        win_rate = (self.wins / self.matches_played * 100) if self.matches_played > 0 else 0
        return (f"Genome(fitness={self.fitness:.2f}, "
                f"W/D/L={self.wins}/{self.draws}/{self.losses}, "
                f"winrate={win_rate:.1f}%)")
