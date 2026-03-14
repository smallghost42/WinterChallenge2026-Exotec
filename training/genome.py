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
    "DEAD":            (-2.0e9, -1.0e5, -1.0e9),
    "FALL_OUT":        (-2.0e9, -1.0e5, -8.0e8),
    "SPACE_CELL":      (0.0, 100.0, 15.0),
    "TRAP_TINY":       (-2.0e5, 0.0, -70000.0),
    "TRAP_SMALL":      (-1.0e5, 0.0, -20000.0),
    "TRAP_MED":        (-5.0e4, 0.0, -4000.0),
    "FALL_ROW":        (-2000.0, 0.0, -320.0),
    "GROUNDED":        (0.0, 2000.0, 300.0),
    "LAND_FOOD":       (0.0, 50000.0, 6000.0),
    "KILL_BONUS":      (0.0, 100000.0, 28000.0),
    "DEATH_PEN":       (-1.0e6, 0.0, -280000.0),
    "CLASH_PEN":       (-20000.0, 0.0, -3500.0),
    "UNDER_ENEMY":     (-2.0e5, 0.0, -50000.0),
    "OPP_CROWD":       (-10000.0, 0.0, -1200.0),
    "SIZE_ADV":        (-2000.0, 2000.0, 180.0),
    "EAT_BONUS":       (0.0, 2.0e5, 90000.0),
    "FOOD_SCALE":      (0.0, 30000.0, 7500.0),
    "CHAIN_FOOD":      (0.0, 15000.0, 3000.0),
    "FOOD_RACE":       (0.0, 15000.0, 2000.0),
    "MOBILITY":        (0.0, 500.0, 55.0),
    "CENTER_PEN":      (-200.0, 0.0, -3.0),
    "TOP_PEN":         (-10000.0, 0.0, -550.0),
    "EDGE_PEN":        (-5000.0, 0.0, -110.0),
    "LADDER":          (0.0, 5000.0, 800.0),
    "ALLY_PEN":        (-5.0e5, 0.0, -110000.0),
    "FAST_SOLID_SUP":  (-5000.0, 5000.0, 200.0),
    "FAST_BODY_SUP":   (-5000.0, 5000.0, 800.0),
    "FAST_FALL_PEN":   (-5000.0, 0.0, -350.0),
    "FAST_BOT_PEN":    (-30000.0, 0.0, -5000.0),
    "FAST_THREAT_PEN": (-10000.0, 0.0, -1600.0),
    "FAST_CROWD_PEN":  (-5000.0, 0.0, -400.0),
    "FAST_FOOD_BONUS": (0.0, 50000.0, 5000.0),
    "FAST_FOOD_PULL":  (0.0, 5000.0, 500.0),
    "FAST_ALLY_PEN":   (-50000.0, 0.0, -8000.0),
    "FAST_TOP_PEN":    (-10000.0, 0.0, -400.0),
    "FAST_EDGE_PEN":   (-5000.0, 0.0, -80.0),
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
