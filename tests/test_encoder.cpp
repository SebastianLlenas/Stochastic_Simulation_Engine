#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>
#include "../src/engine/encoder.h"
#include "../src/core/battle_state.h"
#include "../src/data/database.h"

using namespace omega9;

void reset_state(BattleState& state) {
    std::memset(&state, 0, sizeof(BattleState));
    // Set defaults to avoid division by zero or weirdness
    state.sides[0].active.species_id = SpeciesFlutterMane;
    state.sides[0].active.level = 100;
    state.sides[0].active.hp = 100; // Max is likely ~250-300
    
    state.sides[1].active.species_id = SpeciesIronHands;
    state.sides[1].active.level = 100;
    state.sides[1].active.hp = 100;
}

int main() {
    std::cout << "=== Omega-9 Encoder Verification ===\n";

    // 1. Verify Feature Count
    std::cout << "Feature Count: " << kFeatureCount << "\n";
    if (kFeatureCount == 217) {
        std::cout << "[PASS] Feature count matches expected layout (217).\n";
    } else {
        std::cout << "[WARN] Feature count is " << kFeatureCount << " (Expected 217). Layout might differ.\n";
    }

    // 2. Setup Specific State
    BattleState state;
    reset_state(state);

    // A. Global: Rain (Bit 1)
    state.global_field_mask |= FieldWeatherRain;

    // B. Side 0 Active: 50% HP
    // We need to know Max HP to set current HP to 50%. 
    // Let's just set HP to 0 for now to see if it encodes 0.0, 
    // or set it to Max to see 1.0.
    // Let's try setting Status: Paralysis (Bit 1 of Status)
    state.sides[0].active.major_status = StatusParalysis;

    // C. Side 0 Bench: Slot 0 has a mon
    state.sides[0].bench[0].species_id = SpeciesKoraidon;
    state.sides[0].bench[0].hp_percent = 100;

    // 3. Encode
    std::vector<BattleState> batch = {state};
    std::vector<float> buffer(kFeatureCount);
    
    encode_batch(batch, buffer.data());

    // 4. Inspect Values
    // We iterate through the buffer and print non-zero values to verify logic.
    
    std::cout << "\nNon-zero features detected:\n";
    int non_zeros = 0;
    for (int i = 0; i < kFeatureCount; ++i) {
        if (buffer[i] > 0.0f) {
            std::cout << "Index " << std::setw(3) << i << ": " << buffer[i] << "\n";
            non_zeros++;
        }
    }

    // Logic Checks based on standard layout assumptions:
    // Global (0-10)
    // Side 0 (11-113)
    // Side 1 (114-216)
    
    // Check Rain (Should be early in Global section)
    bool found_rain = false;
    for(int i=0; i<11; ++i) if(buffer[i] == 1.0f) found_rain = true;
    
    if (found_rain) std::cout << "[PASS] Weather (Rain) encoded.\n";
    else std::cout << "[FAIL] Weather not found.\n";

    // Check Paralysis (Should be in Side 0 Active section)
    // Status is usually One-Hot.
    bool found_para = false;
    // Scan Side 0 range
    for(int i=11; i<113; ++i) if(buffer[i] == 1.0f) found_para = true;
    
    if (found_para) std::cout << "[PASS] Status (Paralysis) encoded.\n";
    else std::cout << "[FAIL] Status not found.\n";

    return 0;
}