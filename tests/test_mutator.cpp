#include <iostream>
#include <cstring>
#include "../src/engine/mutator.h"
#include "../src/data/database.h"

using namespace omega9;

void reset_state(BattleState& state) {
    std::memset(&state, 0, sizeof(BattleState));
    state.sides[0].active.hp = 300;
    state.sides[1].active.hp = 300;
    state.sides[0].active.species_id = SpeciesFlutterMane; // Ghost/Fairy
    state.sides[1].active.species_id = SpeciesIronHands;   // Fighting/Electric
}

int main() {
    std::cout << "=== Omega-9 Mutator Verification ===\n";

    BattleState state;
    reset_state(state);

    // 1. Test Damage Application
    StateMutator::apply_damage(state, 0, 100, false, false);
    if (state.sides[0].active.hp == 200) {
        std::cout << "[PASS] Damage applied correctly.\n";
    } else {
        std::cout << "[FAIL] Damage math wrong.\n";
        return 1;
    }

    // 2. Test Fainting (Clamp at 0)
    StateMutator::apply_damage(state, 0, 500, false, false);
    if (state.sides[0].active.hp == 0) {
        std::cout << "[PASS] HP clamped at 0 (Fainted).\n";
    } else {
        std::cout << "[FAIL] HP did not clamp at 0.\n";
        return 1;
    }

    // 3. Test Status Immunity (Electric vs Paralysis)
    // Iron Hands is Electric. Should be immune.
    StateMutator::apply_status(state, 1, StatusParalysis, 0.5f);
    if (state.sides[1].active.major_status == 0) {
        std::cout << "[PASS] Electric type immune to Paralysis.\n";
    } else {
        std::cout << "[FAIL] Immunity check failed.\n";
        return 1;
    }

    // 4. Test Valid Status (Sleep)
    // Iron Hands can sleep.
    StateMutator::apply_status(state, 1, StatusSleep, 0.1f); // Low seed -> 2 turns
    if (state.sides[1].active.major_status == StatusSleep && state.sides[1].active.sleep_turns >= 2) {
        std::cout << "[PASS] Sleep applied with turns.\n";
    } else {
        std::cout << "[FAIL] Sleep application failed.\n";
        return 1;
    }

    // 5. Test Secondary Effects (Moonblast SpA Drop)
    // Seed 0.1 (10%) < 30% -> Should trigger
    StateMutator::apply_secondary_effects(state, 0, MoveMoonblast, 0.1f);
    if (state.sides[1].active.stat_stages[StatSpa] == -1) {
        std::cout << "[PASS] Secondary effect triggered (SpA drop).\n";
    } else {
        std::cout << "[FAIL] Secondary effect failed to trigger.\n";
        return 1;
    }

    // Seed 0.9 (90%) > 30% -> Should NOT trigger
    StateMutator::apply_secondary_effects(state, 0, MoveMoonblast, 0.9f);
    if (state.sides[1].active.stat_stages[StatSpa] == -1) { // Still -1 from before
        std::cout << "[PASS] Secondary effect correctly ignored on high seed.\n";
    } else {
        std::cout << "[FAIL] Secondary effect triggered when it shouldn't.\n";
        return 1;
    }

    return 0;
}
