#include <iostream>
#include <cassert>
#include <cstring>
#include "../src/engine/action.h"
#include "../src/data/database.h"

using namespace omega9;

void reset_state(BattleState& state) {
    std::memset(&state, 0, sizeof(BattleState));
    // Set default HP so they aren't "fainted"
    state.sides[0].active.hp = 100;
    state.sides[1].active.hp = 100;
}

int main() {
    std::cout << "=== Omega-9 Action Logic Verification ===\n";

    BattleState state;
    reset_state(state);

    // 1. Test Speed Calculation (Flutter Mane vs Iron Hands)
    // Flutter Mane (Base 135) vs Iron Hands (Base 50)
    state.sides[0].active.species_id = SpeciesFlutterMane;
    state.sides[1].active.species_id = SpeciesIronHands;

    uint16_t s1 = calculate_speed(state, 0);
    uint16_t s2 = calculate_speed(state, 1);
    
    std::cout << "Flutter Speed: " << s1 << " | Iron Hands Speed: " << s2 << "\n";
    if (s1 == 135 && s2 == 50) {
        std::cout << "[PASS] Base Speed calculation correct.\n";
    } else {
        std::cout << "[FAIL] Base Speed incorrect.\n";
        return 1;
    }

    // 2. Test Choice Scarf
    state.sides[1].active.item_id = ItemChoiceScarf; // Give Iron Hands Scarf
    s2 = calculate_speed(state, 1);
    std::cout << "Scarf Iron Hands Speed: " << s2 << "\n";
    if (s2 == 75) { // 50 * 1.5 = 75
        std::cout << "[PASS] Choice Scarf boost correct.\n";
    } else {
        std::cout << "[FAIL] Choice Scarf logic broken.\n";
        return 1;
    }

    // 3. Test Priority (Quick Attack vs Thunderbolt)
    // Assume MoveThunderPunch (Priority 0) vs MoveCollisionCourse (Priority 0)
    // Let's mock a priority move scenario by manually checking the logic
    // (Since we don't have a priority move in dummy data yet, we rely on the code logic)
    
    Action a1 = Action::make_move(0, MoveMoonblast, false); // Prio 0, Speed 135
    Action a2 = Action::make_move(1, MoveThunderPunch, false); // Prio 0, Speed 75

    if (goes_first(state, a1, a2, 0.5f)) {
        std::cout << "[PASS] Faster mon goes first (Standard).\n";
    } else {
        std::cout << "[FAIL] Speed check failed.\n";
        return 1;
    }

    // 4. Test Trick Room
    state.global_field_mask |= FieldRoomTrick;
    if (!goes_first(state, a1, a2, 0.5f)) { // Should return false (slower goes first)
        std::cout << "[PASS] Trick Room reverses speed.\n";
    } else {
        std::cout << "[FAIL] Trick Room logic failed.\n";
        return 1;
    }

    // 5. Test Switch Priority
    state.global_field_mask &= ~FieldRoomTrick; // Turn off TR
    Action switch_action = Action::make_switch(1, 0); // Side 1 switches
    
    // Even though Side 0 is faster (135 vs 75), Side 1 is switching (Prio 6)
    if (!goes_first(state, a1, switch_action, 0.5f)) {
        std::cout << "[PASS] Switch beats Move.\n";
    } else {
        std::cout << "[FAIL] Switch priority failed.\n";
        return 1;
    }

    return 0;
}