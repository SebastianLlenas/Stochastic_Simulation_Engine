#include <iostream>
#include <cstring>
#include <cmath>
#include "../src/engine/step.h"
#include "../src/data/database.h"
#include "../src/engine/mutator.h"

using namespace omega9;

void reset_state(BattleState& state) {
    std::memset(&state, 0, sizeof(BattleState));
    // P1: Flutter Mane (Fast)
    state.sides[0].active.species_id = SpeciesFlutterMane;
    state.sides[0].active.level = 100;
    state.sides[0].active.hp = 1000; // Increased to survive hits
    
    // P2: Iron Hands (Slow)
    state.sides[1].active.species_id = SpeciesIronHands;
    state.sides[1].active.level = 100;
    state.sides[1].active.hp = 1000; // Increased to survive Moonblast (~430 dmg)
}

int main() {
    std::cout << "=== Omega-9 Step Function Verification ===\n";

    BattleState state;
    reset_state(state);

    // 1. Test Basic Turn (Fast moves first)
    // P1 uses Moonblast, P2 uses Thunder Punch
    Action a1 = Action::make_move(0, MoveMoonblast, false);
    Action a2 = Action::make_move(1, MoveThunderPunch, false);

    const uint16_t p1_start = state.sides[0].active.hp;
    const uint16_t p2_start = state.sides[1].active.hp;
    auto [next_state, reward] = step(state, a1, a2, 0.5f);

    // Flutter Mane (135 Spe) > Iron Hands (50 Spe)
    // So P1 moves first.
    // P1 deals damage. P2 deals damage.
    
    if (next_state.sides[1].active.hp < p2_start &&
        next_state.sides[0].active.hp < p1_start) {
        std::cout << "[PASS] Both mons took damage.\n";
    } else {
        std::cout << "[FAIL] Damage not applied correctly.\n";
        return 1;
    }

    // 2. Test Faint Skip
    // Give P2 1 HP. P1 should kill it. P2 should NOT move.
    state.sides[1].active.hp = 1;
    auto [faint_state, r2] = step(state, a1, a2, 0.5f);
    
    if (faint_state.sides[1].active.hp == 0) {
        std::cout << "[PASS] Target fainted.\n";
    } else {
        std::cout << "[FAIL] Target did not faint.\n";
        return 1;
    }

    // P2 should NOT have moved. P1 should be full HP (unchanged).
    if (faint_state.sides[0].active.hp == p1_start) {
        std::cout << "[PASS] Fainted mon did not move (Turn Skipped).\n";
    } else {
        std::cout << "[FAIL] Fainted mon attacked! (HP: " << faint_state.sides[0].active.hp << ")\n";
        return 1;
    }

    // 3. Test End of Turn (Leftovers)
    reset_state(state);
    state.sides[0].active.hp = 100; // Damaged
    state.sides[0].active.item_id = ItemLeftovers;
    
    // Use dummy moves (Switch to self or invalid move to pass turn)
    // Actually, let's just use moves that miss or do 0 damage?
    // Or just check if HP increased relative to expected.
    // Let's use a move that does 0 damage (e.g. Splash? We don't have it).
    // Let's just use the same moves but check the math.
    
    // P1 uses Moonblast. P2 uses Thunder Punch.
    // P1 takes damage from Thunder Punch.
    // Then P1 heals from Leftovers.
    // This is hard to isolate.
    
    // Better: P2 is fainted. P1 passes turn.
    state.sides[1].active.hp = 0; 
    // P1 uses Moonblast (valid). P2 uses MoveNone (invalid -> fallback).
    // P1 attacks P2 (already 0 HP -> no effect).
    // End of turn -> Leftovers.
    
    auto [heal_state, r3] = step(state, a1, a2, 0.5f);
    
    // Max HP for Flutter Mane ~272 (at lvl 100, 85 EVs).
    // 1/16 of 272 = 17.
    // Start HP 100 -> End HP 117.
    if (heal_state.sides[0].active.hp > 100) {
        std::cout << "[PASS] Leftovers healed.\n";
    } else {
        std::cout << "[FAIL] Leftovers did not heal.\n";
        return 1;
    }

    // 4. Test Win Condition
    // P2 has 0 HP active, and 0 bench.
    // P1 has alive active.
    // Reward should be 1.0.
    if (std::abs(r3 - 1.0f) < 0.001f) {
        std::cout << "[PASS] Win condition detected (+1.0).\n";
    } else {
        std::cout << "[FAIL] Win condition failed (Reward: " << r3 << ")\n";
        return 1;
    }

    return 0;
}
