# Omega-9: Stochastic Simulation & RL Environment

**Status:** [WIP] Phase 1 - Core Engine Stabilization  
**Target:** Gen 9 Random Battles (Stochastic Game Theory Matrix)  
**Stack:** C++23, OpenMP, CMake, Python (Bindings)  

## 1. Overview
Omega-9 is a parallelized C++ state-machine designed to model complex, turn-based game theory mechanics with high hidden-information asymmetry. It utilizes a data-oriented, bitboard-based physics engine optimized for massive parallel simulation. The architecture serves as a high-speed environment for Reinforcement Learning (RL) and Monte Carlo Tree Search (MCTS) self-play batching.

## 2. Architectural Constraints
The engine is built on three strict performance pillars:

* **Memory Optimization (Bitboards):** No standard OOP overhead. The battle state is a fixed-size, trivially copyable `struct` (<4KB) utilizing `uint64_t` bitmasks and flat arrays. Designed to fit the active state within CPU L1/L2 cache for instantaneous hashing.
* **Deterministic Execution:** The engine operates as a pure mathematical function: `f(State, Action1, Action2, Seed) -> (NextState, Reward)`. Zero side effects. All complex logic (damage formulas, type multipliers) is pre-computed into static `constexpr` arrays for O(1) lookups.
* **Vectorization & Batching:** Data structures are SIMD-aligned. The primary API utilizes `step_batch()` via OpenMP to advance N games simultaneously across multiple CPU threads.

## 3. Technical Specifications
* **Memory:** Strict usage of `std::array`. Zero heap allocation (`std::vector`, `std::string`) inside the hot loop.
* **Throughput Target:** >100,000 turns per second per CPU core.
* **Precision:** Prioritizes throughput over perfect accuracy (e.g., approximating damage ranges into normalized buckets).

## 4. Endgame Oracle (Tablebase)
The engine interfaces with a 50GB+ Endgame Tablebase. In 1v1 state transitions, the engine acts as an Oracle, returning a deterministic Win/Loss reward immediately to bypass the standard simulation loop and preserve compute bandwidth.

---

### Deployment Note
*This repository is a professional mirror of the core architecture. As the Systems Architect and QA Lead, I defined the mathematical constraints, memory layouts, and state-space logic outlined above, utilizing automated LLM pipelines for raw C++ syntax generation.* 

*Current active development is strictly focused on black-box QA: identifying logic collisions within the generated code, patching memory leaks, and stabilizing the core physics engine prior to neural network integration.*