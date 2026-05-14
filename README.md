\# Omega-9: Superhuman Pokémon Battle Engine



\*\*Version:\*\* 0.1.0 (Pre-Alpha)

\*\*Target:\*\* Pokémon Gen 9 Random Battles

\*\*Language:\*\* C++23 (Engine), Python/JAX (Training)

\*\*Architecture:\*\* Bitboard-based, Stateless, Vectorized Reinforcement Learning Environment



\## 1. Project Overview

Omega-9 is a high-performance, superhuman AI project designed to solve Generation 9 Random Battles. Unlike traditional bots that rely on object-oriented state representations and heuristic search (Minimax/MCTS), Omega-9 utilizes a \*\*Bitboard-based physics engine\*\* optimized for massive parallel simulation on GPUs.



The goal is to train a Deep Reinforcement Learning agent (AlphaZero/MuZero style) capable of executing \*\*>1,000,000 steps per second\*\* during self-play training.



\## 2. Core Architecture

The engine is built on three non-negotiable pillars. All code contributions must adhere to these principles:



\### A. Bitboard Representation (The "Physics")

\*   \*\*No Objects:\*\* We do not use classes like `class Pokemon { int hp; }`.

\*   \*\*Data-Oriented Design:\*\* The entire battle state must fit into a fixed-size `struct` (target < 4KB) composed of `uint64\_t` bitmasks and flat arrays.

\*   \*\*Why:\*\* To fit the entire state in CPU L1/L2 cache and allow for instant copying/hashing.



\### B. Stateless Logic (The "Rules")

\*   \*\*Pure Functions:\*\* The engine logic is a pure function: $f(State\_t, Action\_{p1}, Action\_{p2}, Seed) \\rightarrow (State\_{t+1}, Reward)$.

\*   \*\*No Side Effects:\*\* The engine does not maintain internal history or logs. It simply mutates the bitboard.

\*   \*\*Lookup Tables:\*\* Complex logic (Damage formulas, Type charts) is pre-computed into static `constexpr` arrays or flat hashmaps. We do not calculate `((2 \* Level / 5 + 2)...` at runtime.



\### C. Vectorization \& Batching (The "Scale")

\*   \*\*SIMD First:\*\* Data structures must be aligned for SIMD operations.

\*   \*\*Batch Processing:\*\* The primary API is `step\_batch()`, which advances N games simultaneously (where N = 10,000+).



\## 3. Technical Specifications



\### Tech Stack

\*   \*\*Core Engine:\*\* C++23 (Clang/GCC latest).

\*   \*\*Build System:\*\* CMake.

\*   \*\*Parallelism:\*\* OpenMP / CUDA (future).

\*   \*\*Python Bindings:\*\* Pybind11 or Nanobind (for JAX/PyTorch integration).



\### Constraints

1\.  \*\*Memory:\*\* `BattleState` struct must be `trivially\_copyable`. No `std::vector`, `std::string`, or heap allocation inside the hot loop. Use `std::array`.

2\.  \*\*Speed:\*\* A single CPU core must be able to simulate >100,000 turns per second.

3\.  \*\*Precision:\*\* We prioritize \*\*Speed\*\* over \*\*Perfect Accuracy\*\*.

&nbsp;   \*   \*Acceptable:\* Approximating damage ranges into buckets.

&nbsp;   \*   \*Unacceptable:\* Parsing strings or JSON during the battle loop.



\## 4. Directory Structure

```text

Omega9\_Engine/

│

├── reference/                <-- PUT THE SHOWDOWN FILES HERE

│   ├── data/

│   │   ├── pokedex.ts

│   │   ├── moves.ts

│   │   ├── typechart.ts

│   │   ├── conditions.ts

│   │   └── random-battles/

│   │       └── gen9/

│   │           ├── sets.json

│   │           └── teams.ts

│   └── sim/

│       ├── battle.ts

│       ├── pokemon.ts

│       ├── side.ts

│       ├── field.ts

│       ├── battle-queue.ts

│       └── battle-actions.ts

├── src/

│   ├── core/       # Bitboard structs and State definitions

│   ├── data/       # Static lookup tables (Moves, Species, TypeChart)

│   ├── engine/     # Mechanics (Damage, Turn Order, Side Effects)

│   ├── env/        # RL Environment wrapper (Step, Reset, Reward)

│   └── python/     # Python bindings
├── tests/          # Unit tests comparing against Showdown logic

└── tools/          # Scripts to parse /data into C++ headers

```



\## 5. The "1v1 Tablebase" Integration

\*Note for Developers:\*

This engine is designed to interface with a \*\*50GB+ Endgame Tablebase\*\*.

\*   The engine must detect when a state transitions from 2v1 to 1v1.

\*   In 1v1 states, the engine acts as an Oracle, returning a deterministic Win/Loss reward immediately, bypassing the standard simulation loop.



\## 6. Development Roadmap (Prompt Sequence)

The codebase is being constructed via a sequence of 10 specific modules:

1\.  \*\*Bitboard State Definition\*\* (Memory layout)

2\.  \*\*Static Database\*\* (O(1) Lookups)

3\.  \*\*Action \& Turn Order\*\* (Priority logic)

4\.  \*\*Vectorized Damage\*\* (SIMD math)

5\.  \*\*State Mutation\*\* (Bitwise operations)

6\.  \*\*Switching \& Hazards\*\* (Entry logic)

7\.  \*\*The Step Function\*\* (Main loop)

8\.  \*\*Batch Processing\*\* (OpenMP)

9\.  \*\*Tensor Serialization\*\* (Input for Neural Net)

10\. \*\*Validation Suite\*\* (Correctness tests)



---

\*This README serves as the context for all code generation. If a generated solution violates the "No Heap Allocation" or "Bitboard" constraints, it is invalid.\*

