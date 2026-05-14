#include <iostream>
#include <cstring>
#include <algorithm>
#include "../src/engine/damage.h"
#include "../src/data/database.h"

using namespace omega9;

void reset_state(BattleState& state) {
    std::memset(&state, 0, sizeof(BattleState));
    state.sides[0].active.level = 100;
    state.sides[1].active.level = 100;
}

int main() {
    std::cout << "=== Omega-9 Damage Verification ===\n";

    BattleState state;
    reset_state(state);

    // Setup Flutter Mane vs Koraidon
    state.sides[0].active.species_id = SpeciesFlutterMane;
    state.sides[1].active.species_id = SpeciesKoraidon;

    // 1. Test Basic STAB + Super Effective (Moonblast vs Koraidon)
    // Fairy vs Fighting/Dragon = 4x Effective
    // STAB = 1.5x
    // Base Power = 95
    DamageResult res = calculate_damage(state, 0, MoveMoonblast, false);
    
    std::cout << "Moonblast Rolls: ";
    for (auto r : res.rolls) std::cout << r << " ";
    std::cout << "\n";

    // Sanity Check: Should be massive damage
    if (res.rolls[0] > 300) {
        std::cout << "[PASS] Super Effective damage looks correct (High).\n";
    } else {
        std::cout << "[FAIL] Damage too low for 4x effective move!\n";
        return 1;
    }

    // 2. Test Physical/Special Split (Collision Course vs Flutter Mane)
    // Fighting vs Ghost/Fairy = 0x (Immune)
    DamageResult res_immune = calculate_damage(state, 1, MoveCollisionCourse, false);
    if (res_immune.rolls[15] == 0) {
        std::cout << "[PASS] Immunity logic correct (0 damage).\n";
    } else {
        std::cout << "[FAIL] Immunity failed! Dealt " << res_immune.rolls[15] << "\n";
        return 1;
    }

    // 3. Test Stat Stages (+2 SpA)
    state.sides[0].active.stat_stages[StatSpa] = 2; // +2
    DamageResult res_boosted = calculate_damage(state, 0, MoveMoonblast, false);
    
    // +2 should be roughly double damage
    if (res_boosted.rolls[0] > res.rolls[0] * 1.8) {
        std::cout << "[PASS] +2 Stat Boost applied correctly.\n";
    } else {
        std::cout << "[FAIL] Stat boost logic failed.\n";
        return 1;
    }

    // 4. Test Psyshock (Special Attacker vs Physical Defense)
    // Flutter Mane (High SpA) vs Iron Hands (High Def, Low SpD)
    state.sides[1].active.species_id = SpeciesIronHands;
    DamageResult res_psyshock = calculate_damage(state, 0, MovePsyshock, false);
    
    // Psyshock hits Def. Iron Hands has high Def.
    // Shadow Ball hits SpD. Iron Hands has low SpD.
    DamageResult res_shadowball = calculate_damage(state, 0, MoveShadowBall, false);

    std::cout << "Psyshock (vs Def): " << res_psyshock.rolls[15] << "\n";
    std::cout << "Shadow Ball (vs SpD): " << res_shadowball.rolls[15] << "\n";

    // Shadow Ball should do MORE damage because Iron Hands SpD < Def
    if (res_shadowball.rolls[15] > res_psyshock.rolls[15]) {
        std::cout << "[PASS] Psyshock correctly targets Defense.\n";
    } else {
        std::cout << "[FAIL] Psyshock logic incorrect.\n";
        return 1;
    }

    return 0;
}