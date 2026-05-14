#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include "../src/core/battle_state.h"

using namespace omega9;

int main() {
    std::cout << "=== Omega-9 State Verification ===\n";

    // 1. Verify Size
    size_t size = sizeof(BattleState);
    std::cout << "BattleState Size: " << size << " bytes" << std::endl;
    if (size <= 4096) {
        std::cout << "[PASS] Size is under 4KB limit.\n";
    } else {
        std::cout << "[FAIL] Size exceeds 4KB limit!\n";
        return 1;
    }

    // 2. Verify Alignment
    size_t align = alignof(BattleState);
    std::cout << "BattleState Alignment: " << align << " bytes" << std::endl;
    if (align == 64) {
        std::cout << "[PASS] Alignment is optimized for Cache Lines (64 bytes).\n";
    } else {
        std::cout << "[WARN] Alignment is not 64 bytes. Performance may degrade.\n";
    }

    // 3. Verify Trivial Copyability (The most important check for Bitboards)
    if (std::is_trivially_copyable_v<BattleState>) {
        std::cout << "[PASS] State is Trivially Copyable (memcpy safe).\n";
    } else {
        std::cout << "[FAIL] State is NOT Trivially Copyable! Remove complex objects.\n";
        return 1;
    }

    // 4. Bit Manipulation Sanity Check
    BattleState state;
    // Clear memory (simulating a fresh state)
    std::memset(&state, 0, sizeof(BattleState));
    
    // Set Sun and Stealth Rock
    state.global_field_mask |= FieldWeatherSun;
    state.hazard_mask[0] |= HazardStealthRock;

    if ((state.global_field_mask & FieldWeatherSun) && 
        (state.hazard_mask[0] & HazardStealthRock)) {
        std::cout << "[PASS] Bitmasks are functioning correctly.\n";
    } else {
        std::cout << "[FAIL] Bitmask logic is broken.\n";
        return 1;
    }

    return 0;
}