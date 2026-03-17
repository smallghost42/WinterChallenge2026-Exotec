"""
Genome definition for the genetic training system.

Each genome represents a set of tunable parameters for the bot.
Parameters are stored as a dictionary mapping parameter names to float values.
"""

import json
import random
import copy

# Parameter definitions: (min, max, default)
# These match the tunable double parameters in bot.cpp's loadParamsJson map.
PARAM_DEFS = {
    # Core evaluation - length difference
    "LEN_WEIGHT":                (20.0,   800.0,  160.0),
    "LEN_WEIGHT_SMALL":          (20.0,  1000.0,  200.0),
    "LEN_WEIGHT_WINNING_LATE":   (20.0,  1500.0,  280.0),
    "LEN_WEIGHT_LOSING_LATE":    (10.0,   600.0,  100.0),

    # Apple pursuit / growth
    "GROWTH_BASE":               (20.0,  1200.0,  220.0),
    "SCARCITY_MULT_LOW":         (1.0,    10.0,     2.5),
    "SCARCITY_MULT_MED":         (1.0,     8.0,     2.0),
    "SCARCITY_MULT_HIGH":        (0.5,     6.0,     1.5),
    "VORONOI_APPLE_CLEAR":       (0.0,    10.0,     1.2),
    "VORONOI_APPLE_SLIGHT":      (0.0,     6.0,     0.8),
    "VORONOI_APPLE_CONTESTED":   (-5.0,   5.0,     0.4),
    "VORONOI_APPLE_LOSE":        (-100.0,  0.0,    -5.0),
    "VORONOI_APPLE_TIE_LOSE":    (-100.0,  0.0,    -5.0),
    "EAT_BONUS":                 (0.0,  1.0e5,  20000.0),

    # Voronoi territory
    "TERRITORY_WEIGHT":          (0.0,    10.0,     1.0),
    "TERRITORY_WEIGHT_SMALL":    (0.0,    15.0,     1.5),
    "TERRITORY_WEIGHT_SMALL_LATE": (0.0, 25.0,     3.0),
    "TERRITORY_WEIGHT_WINNING_LATE": (0.0, 15.0,   1.5),
    "ENERGY_CONTROL_WEIGHT":     (0.0,   400.0,   40.0),
    "CLOSEST_ENERGY_WEIGHT":     (0.0,   800.0,   80.0),
    "CLOSEST_ENERGY_FALLBACK":   (0.0,   400.0,   40.0),
    "NO_ENERGY_PEN":             (-600.0,  0.0,   -50.0),

    # Safety / trap detection
    "TRAP_SEVERE":               (-1200.0, 0.0,  -120.0),
    "TRAP_MILD":                 (-200.0,  0.0,   -15.0),
    "TRAP_MULT_SMALL":           (0.5,    15.0,     2.0),
    "TRAP_MULT_TINY":            (0.5,    20.0,     3.0),
    "OPP_TRAP_BONUS":            (0.0,   800.0,   60.0),
    "OPP_TRAP_BONUS_SMALL":      (0.0,  1200.0,  100.0),
    "LOG_SPACE_BONUS":           (0.0,    20.0,    1.5),
    "SPACE_GOOD":                (0.0,   600.0,   50.0),
    "SPACE_BAD":                 (-600.0,  0.0,  -60.0),

    # Valid moves penalty
    "NO_MOVES_PEN":              (-2000.0, 0.0,  -200.0),
    "NO_MOVES_PEN_SMALL":        (-4000.0, 0.0,  -400.0),
    "ONE_MOVE_PEN":              (-600.0,  0.0,   -50.0),
    "ONE_MOVE_PEN_SMALL":        (-1200.0, 0.0,  -100.0),
    "TWO_MOVES_PEN_SMALL":       (-200.0,  0.0,   -20.0),
    "MOBILITY_BONUS":            (0.0,    60.0,    3.0),

    # Head collision
    "HEAD_CLOSE_SMALLER":        (-8000.0, 0.0,  -800.0),
    "HEAD_CLOSE_SMALLER_SMALL":  (-2000.0, 0.0,  -200.0),
    "HEAD_CLOSE_BIGGER":         (0.0,  1000.0,   50.0),
    "HEAD_CLOSE_BIGGER_SMALL":   (0.0,   600.0,   30.0),
    "HEAD_NEAR_SMALLER":         (-400.0,  0.0,   -20.0),
    "HEAD_NEAR_SMALLER_SMALL":   (-800.0,  0.0,   -80.0),
    "HEAD_NEAR_LINE_SMALL":      (-400.0,  0.0,   -40.0),

    # Edge penalties
    "EDGE_X":                    (-200.0,  0.0,   -15.0),
    "EDGE_Y":                    (-200.0,  0.0,   -10.0),
    "CORNER_EXTRA":              (-400.0,  0.0,   -25.0),

    # Gravity
    "GRAVITY_RISK_NONE":         (-3000.0, 0.0,  -300.0),
    "GRAVITY_RISK_HIGH":         (-400.0,  0.0,   -30.0),
    "GRAVITY_RISK_LOW":          (-200.0,  0.0,   -10.0),
    "GRAVITY_DEATH":             (-4000.0, 0.0,  -500.0),
    "GRAVITY_EXPLOIT":           (0.0,    20.0,    1.0),
    "GRAVITY_EXPLOIT_FALL_DEATH": (0.0, 2000.0,  200.0),
    "GRAVITY_EXPLOIT_FALL_FAR":  (0.0,   200.0,   15.0),
    "GRAVITY_RISK_SMALL":        (0.1,     6.0,    0.8),
    "GRAVITY_RISK_LARGE":        (0.1,     4.0,    0.5),

    # Anti-stall
    "ANTI_TRAMPOLINE":           (-12000.0, 0.0, -1200.0),
    "STALL_REVISIT":             (-20000.0, 0.0, -2000.0),

    # Alive count
    "ALIVE_EARLY":               (0.0,  1200.0,  120.0),
    "ALIVE_MID":                 (0.0,   800.0,   80.0),
    "ALIVE_LATE":                (0.0,   400.0,   40.0),
    "ALIVE_WINNING_MULT":        (0.5,     8.0,    1.5),
    "ALIVE_SMALL_MULT":          (0.5,     8.0,    1.3),

    # Tail chase
    "TAIL_DEFAULT":              (0.0,   100.0,    5.0),
    "TAIL_WINNING":              (0.0,   400.0,   40.0),
    "TAIL_NO_FOOD":              (0.0,   350.0,   35.0),
    "TAIL_NO_VORONOI":           (0.0,   300.0,   30.0),
    "TAIL_SMALL_MULT":           (0.5,     8.0,    1.5),

    # Short snake penalty
    "SHORT_3":                   (-1600.0, 0.0,  -150.0),
    "SHORT_3_SMALL":             (-2500.0, 0.0,  -250.0),
    "SHORT_4":                   (-600.0,  0.0,   -40.0),
    "SHORT_4_SMALL":             (-1000.0, 0.0,   -80.0),

    # Staircase bonus
    "STAIRCASE":                 (0.0,   160.0,    8.0),

    # Max len threat
    "OPP_MAXLEN_THREAT":         (-400.0,  0.0,   -20.0),

    # Greedy move heuristic
    "GREEDY_APPLE_IMMEDIATE":    (100.0, 10000.0, 1000.0),
    "GREEDY_APPLE_DIST":         (-200.0,  0.0,   -10.0),
    "GREEDY_SUPPORT":            (0.0,   100.0,    5.0),
    "GREEDY_UP_PEN":             (-40.0,   0.0,   -2.0),
    "GREEDY_BLOCKED_PEN":        (-5000.0, 0.0,  -500.0),
    "GREEDY_HEAD_COLL_PEN":      (-1000.0, 0.0,  -100.0),
    "GREEDY_HEAD_COLL_PEN_SMALL": (-2000.0, 0.0, -200.0),
    "GREEDY_HEAD_COLL_BONUS":    (0.0,   400.0,   30.0),
    "GREEDY_TRAPPED_PEN":        (-3000.0, 0.0,  -300.0),
    "GREEDY_TRAPPED_MILD_PEN":   (-600.0,  0.0,   -50.0),

    # Beam search
    "BEAM_TRAPPED_PEN":          (-50000.0, 0.0, -5000.0),
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
