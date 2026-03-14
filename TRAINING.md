# Genetic Training System

This directory contains an offline genetic training system for evolving optimized bot parameters for the WinterChallenge2026-Exotec snake/bird game.

## Architecture

```
bot/
  bot.cpp              # C++ competition bot with parameterized heuristics
  Makefile             # Builds the bot binary

training/
  genome.py            # Genome definition (15 tunable parameters)
  genetic.py           # Genetic algorithm (selection, crossover, mutation)
  match_runner.py      # Interface to Java simulator for running matches
  train.py             # Main training loop

config/
  optimized_params.json  # Best parameters (output of training)

src/test/java/
  HeadlessRunner.java  # Headless match runner for automated evaluation
```

## Prerequisites

- **Java 17** (for the game simulator)
- **Maven** (to build the Java referee)
- **g++** with C++17 support (to build the bot)
- **Python 3.8+** (for the training scripts)

## Quick Start

### 1. Build the bot

```bash
cd bot
make
```

### 2. Build the Java referee

```bash
mvn compile -Dmaven.test.skip=true
```

### 3. Run training

```bash
cd training
python train.py --generations 30 --population 20 --matches 4
```

### 4. Use optimized parameters

After training, the best parameters are saved to `config/optimized_params.json`.
The bot can load them at runtime:

```bash
./bot/bot --config config/optimized_params.json
```

## Training Options

| Flag | Default | Description |
|------|---------|-------------|
| `--generations` | 30 | Number of evolutionary generations |
| `--population` | 20 | Population size |
| `--matches` | 4 | Matches per opponent per genome |
| `--parallel` | 1 | Parallel match workers |
| `--elite` | 4 | Elite genomes preserved each generation |
| `--mutation-rate` | 0.3 | Probability of mutating each parameter |
| `--mutation-strength` | 0.2 | Relative mutation magnitude |
| `--seed` | None | Random seed for reproducibility |
| `--resume` | None | Path to genome JSON to seed population |
| `--output` | config/optimized_params.json | Output path for best genome |

## Genome Parameters

The bot uses 15 tunable parameters organized into categories:

### Apple Pursuit
- `apple_weight` (0-10, default 3.0): How aggressively to pursue apples
- `apple_dist_decay` (0.01-0.5, default 0.15): Distance decay for apple scoring
- `cluster_bonus` (0-2, default 0.4): Bonus for apple clusters

### Safety
- `safety_weight` (0-10, default 5.0): Overall safety preference
- `dead_end_penalty` (0-20, default 8.0): Penalty for dead-end positions
- `fall_penalty` (0-10, default 2.0): Penalty for moves risking falls
- `wall_penalty` (0-5, default 1.0): Penalty for wall proximity

### Space Control
- `space_weight` (0-10, default 2.0): Value of reachable space
- `height_weight` (0-5, default 0.5): Preference for higher positions
- `center_weight` (0-3, default 0.3): Preference for center positions

### Opponent Interaction
- `opponent_proximity` (0-10, default 3.0): Penalty for being near opponents
- `aggression` (0-5, default 1.0): Aggression when longer than opponent

### Survival
- `survival_priority` (0-10, default 4.0): Priority for survival over collection
- `length_bonus` (0-3, default 0.2): Bonus for length advantage
- `body_block_bonus` (0-3, default 0.5): Bonus for body positioning

## How It Works

### Evaluation
Each genome is evaluated by running multiple matches against:
1. **WaitBot**: The trivial WAIT bot (baseline)
2. **DefaultBot**: Bot with default parameters
3. **PreviousBest**: Best genomes from prior generations

### Fitness Scoring
- **Win**: +3.0 points
- **Draw**: +1.0 points
- **Close loss**: Up to +0.5 points (proportional to score ratio)
- **Survival bonus**: +0.5 for having live birds at end

### Evolution
- **Elitism**: Top performers are preserved unchanged
- **Tournament selection**: Parents chosen by fitness tournament
- **Uniform crossover**: Parameters randomly taken from each parent
- **Gaussian mutation**: Parameters perturbed by Gaussian noise

## Final Competition Bot

The competition bot (`bot/bot.cpp`) is designed to compile as a standalone single-file C++ program:

```bash
g++ -std=c++17 -O2 -o bot bot.cpp
```

For CodinGame submission:
1. Run training to find optimal parameters
2. Update the default values in the `Params` struct in `bot.cpp` with the optimized values
3. Submit `bot.cpp` as a single file (it uses embedded defaults when no `--config` flag is given)

The bot respects CodinGame constraints:
- First turn: < 1000ms
- Subsequent turns: < 50ms
- Single-file C++17 submission
- No external dependencies
