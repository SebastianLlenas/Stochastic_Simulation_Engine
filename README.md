# Omega-9: Stochastic Simulation & RL Environment

> **🟢 PROJECT STATUS: ACTIVE R&D (PHASE 1)**
> *Current Focus: Core physics engine stabilization, memory-leak patching, and logic-collision debugging. Neural Network integration is suspended pending 100% deterministic state-machine validation.*

**Version:** 0.1.0 (Pre-Alpha)  
**Target Matrix:** Turn-Based Stochastic Game Theory (VGC Rule-set)  
**Language:** C++23 (Engine), Python (Training/Bindings)  
**Architecture:** Bitboard-based, Stateless, Vectorized Reinforcement Learning Environment  

## 1. Project Overview

Omega-9 is a high-performance, parallelized state-machine and simulation engine designed to model complex, turn-based game theory mechanics. To stress-test the architecture, the engine is currently configured to model the state-space of the VGC competitive metagame—a highly volatile environment with thousands of variables, hidden information, and RNG mechanics. 

Unlike traditional bots that rely on object-oriented state representations and heuristic search, Omega-9 utilizes a **Data-Oriented, Bitboard-based physics engine** optimized for massive parallel simulation. The architecture is engineered specifically to serve as a high-speed environment for Reinforcement Learning (RL) and Monte Carlo Tree Search (MCTS) algorithms, capable of executing >1,000,000 steps per second during self-play batching.

## 2. Core Architecture Constraints

The engine is built on three non-negotiable pillars to ensure maximum computational efficiency:

### A. Bitboard Representation (Memory Optimization)
* **No Objects:** The architecture strictly prohibits standard OOP overhead (e.g., `class Entity { int hp; }`).
* **Data-Oriented Design:** The entire battle state must fit into a fixed-size, trivially copyable `struct` (target < 4KB) composed of `uint64_t` bitmasks and flat arrays.
* **Objective:** To fit the entire active state within CPU L1/L2 cache, allowing for instantaneous copying and hashing during deep-tree searches.

### B. Stateless Logic (Deterministic Execution)
* **Pure Functions:** The engine logic operates as a pure mathematical function: `f(State_t, Action_p1, Action_p2, Seed) -> (State_t+1, Reward)`.
* **Zero Side Effects:** The engine does not maintain internal history, logs, or external dependencies. It strictly mutates the bitboard.
* **O(1) Lookup Tables:** Complex logic (damage formulas, type multipliers) is pre-computed into static `constexpr` arrays or flat hashmaps to eliminate runtime calculation overhead.

### C. Vectorization & Batching (Scale)
* **SIMD Alignment:** Data structures are aligned for Single Instruction, Multiple Data (SIMD) operations.
* **Batch Processing:** The primary API utilizes `step_batch()`, leveraging OpenMP to advance N games simultaneously across multiple CPU threads.

## 3. Technical Specifications

* **Core Engine:** C++23 (Clang/GCC latest)
* **Build System:** CMake
* **Parallelism:** OpenMP 
* **Python Bindings:** Pybind11 / Nanobind (for JAX/PyTorch integration)

**Performance Constraints:**
1. **Memory:** No `std::vector`, `std::string`, or heap allocation inside the hot loop. Strict usage of `std::array`.
2. **Speed:** A single CPU core must simulate >100,000 turns per second.
3. **Precision:** Speed is prioritized over perfect accuracy (e.g., approximating damage ranges into normalized buckets is acceptable; parsing JSON during the battle loop is prohibited).

## 4. Endgame State Oracle (Tablebase Integration)

The engine is architected to interface with a 50GB+ Endgame Tablebase. 
* The engine detects when a simulation state transitions to a 1v1 matrix.
* In 1v1 states, the engine acts as an Oracle, returning a deterministic Win/Loss reward immediately, bypassing the standard simulation loop to save compute bandwidth.

## 5. Architectural Milestones

The codebase is structured into 10 specific deployment modules:
1. Bitboard State Definition (Memory layout)
2. Static Database (O(1) Lookups)
3. Action & Turn Order (Priority logic)
4. Vectorized Damage (SIMD math)
5. State Mutation (Bitwise operations)
6. Switching & Hazards (Entry logic)
7. The Step Function (Main loop)
8. Batch Processing (OpenMP)
9. Tensor Serialization (Input for Neural Net)
10. Validation Suite (Correctness tests)

---

### 🔒 OPSEC & ARCHITECTURAL NOTE
*This repository is a sanitized, professional mirror of a proprietary architecture. My role in this project is strictly **Systems Architect and QA Lead**. I designed the mathematical constraints, the state-space logic, and the architectural requirements outlined above, and deployed advanced LLM coding agents to generate the compiled C++ syntax.* 

*My active execution loop consists of rigorous black-box QA, identifying logic collisions within the AI-generated code, isolating edge-case failures, and re-prompting the agents to patch memory leaks and stabilize the core physics engine prior to neural network integration.*