#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include "../src/engine/batch.h"
#include "../src/data/database.h"

using namespace omega9;

void setup_state(BattleState& state, int id) {
    std::memset(&state, 0, sizeof(BattleState));
    // Give them different HP so we can track them
    state.sides[0].active.species_id = SpeciesFlutterMane;
    state.sides[0].active.hp = 100 + id; // State 0 = 100hp, State 1 = 101hp...
    state.sides[0].active.level = 100;
    
    state.sides[1].active.species_id = SpeciesIronHands;
    state.sides[1].active.hp = 100 + id;
    state.sides[1].active.level = 100;
}

int main() {
    std::cout << "=== Omega-9 Batch Verification ===\n";

    const int N = 100;
    std::vector<BattleState> states(N);
    std::vector<Action> p1_actions(N);
    std::vector<Action> p2_actions(N);
    std::vector<float> seeds(N);

    // 1. Setup Batch
    for (int i = 0; i < N; ++i) {
        setup_state(states[i], i);
        // Even indices: Attack
        // Odd indices: Switch (to slot 0, assuming bench exists or just testing action processing)
        if (i % 2 == 0) {
            p1_actions[i] = Action::make_move(0, MoveMoonblast, false);
        } else {
            p1_actions[i] = Action::make_move(0, MoveShadowBall, false);
        }
        p2_actions[i] = Action::make_move(1, MoveThunderPunch, false);
        seeds[i] = 0.5f;
    }

    // 2. Run Batch
    auto start = std::chrono::high_resolution_clock::now();
    BatchResult result = step_batch(states, p1_actions, p2_actions, seeds);
    auto end = std::chrono::high_resolution_clock::now();

    // 3. Verify Output Size
    if (result.states.size() != N) {
        std::cout << "[FAIL] Output size mismatch.\n";
        return 1;
    }

    // 4. Verify Independence
    // Check State 0 (HP was 100) vs State 1 (HP was 101)
    // Both took damage. State 1 should still have exactly 1 more HP than State 0 
    // (assuming same damage roll from same seed).
    
    uint16_t hp0 = result.states[0].sides[0].active.hp;
    uint16_t hp1 = result.states[1].sides[0].active.hp;

    std::cout << "State 0 HP: " << hp0 << " (Started 100)\n";
    std::cout << "State 1 HP: " << hp1 << " (Started 101)\n";

    if (hp1 == hp0 + 1) {
        std::cout << "[PASS] Batch states processed independently.\n";
    } else {
        std::cout << "[FAIL] State interference detected or logic mismatch.\n";
        return 1;
    }

    std::chrono::duration<double> diff = end - start;
    std::cout << "[PASS] Processed " << N << " states in " << diff.count() * 1000 << "ms.\n";

    return 0;
}