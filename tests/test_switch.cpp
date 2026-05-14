#include <iostream>
#include <cstring>
#include "../src/engine/switch.h"
#include "../src/data/database.h"
#include "../src/engine/mutator.h"

using namespace omega9;

void reset_state(BattleState& state) {
    std::memset(&state, 0, sizeof(BattleState));
    // Side 0: Active = Flutter Mane, Bench 0 = Koraidon
    state.sides[0].active.species_id = SpeciesFlutterMane;
    state.sides[0].active.level = 100;
    state.sides[0].active.hp = 200; // Arbitrary low number
    
    state.sides[0].bench[0].species_id = SpeciesKoraidon;
    state.sides[0].bench[0].level = 100;
    state.sides[0].bench[0].hp_percent = 100;
}

int main() {
    std::cout << "=== Omega-9 Switch & Hazard Verification ===\n";

    BattleState state;
    reset_state(state);

    // 1. Test Basic Switch
    execute_switch(state, 0, 0);
    
    if (state.sides[0].active.species_id == SpeciesKoraidon &&
        state.sides[0].bench[0].species_id == SpeciesFlutterMane) {
        std::cout << "[PASS] Switch executed correctly.\n";
    } else {
        std::cout << "[FAIL] Switch failed to swap mons.\n";
        return 1;
    }

    // 2. Test Stealth Rock Damage
    reset_state(state);
    state.hazard_mask[0] |= HazardStealthRock;
    
    // A. Execute Switch (Koraidon enters, HP set to Max ~362)
    execute_switch(state, 0, 0);
    uint16_t hp_fresh = state.sides[0].active.hp;
    
    // B. Apply Hazards
    apply_entry_hazards(state, 0);
    uint16_t hp_damaged = state.sides[0].active.hp;
    
    std::cout << "HP Before: " << hp_fresh << " -> After SR: " << hp_damaged << "\n";

    if (hp_damaged < hp_fresh) {
         std::cout << "[PASS] Stealth Rock applied damage.\n";
    } else {
         std::cout << "[FAIL] Stealth Rock did 0 damage.\n";
         return 1;
    }

    // 3. Test Toxic Spikes (Poison Status)
    reset_state(state);
    state.hazard_mask[0] |= HazardToxicSpikes1;
    
    execute_switch(state, 0, 0); // Koraidon in
    apply_entry_hazards(state, 0);
    
    if (state.sides[0].active.major_status == StatusPoison) {
        std::cout << "[PASS] Toxic Spikes applied Poison.\n";
    } else {
        std::cout << "[FAIL] Toxic Spikes failed to poison.\n";
        return 1;
    }

    // 4. Test Heavy Duty Boots
    reset_state(state);
    state.hazard_mask[0] |= HazardStealthRock;
    state.sides[0].bench[0].item_id = ItemHeavyDutyBoots; 
    
    execute_switch(state, 0, 0);
    uint16_t hp_boots_start = state.sides[0].active.hp;
    apply_entry_hazards(state, 0);
    uint16_t hp_boots_end = state.sides[0].active.hp;
    
    if (hp_boots_end == hp_boots_start) {
        std::cout << "[PASS] Heavy Duty Boots prevented damage.\n";
    } else {
        std::cout << "[FAIL] Boots failed (HP dropped from " << hp_boots_start << " to " << hp_boots_end << ").\n";
        return 1;
    }

    return 0;
}