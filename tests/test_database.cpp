#include <iostream>
#include <cassert>
#include <cmath>
#include "../src/data/database.h"

using namespace omega9;

// Helper to check float equality
bool is_close(float a, float b) {
    return std::abs(a - b) < 0.001f;
}

int main() {
    std::cout << "=== Omega-9 Database Verification ===\n";

    // 1. Access Singleton
    const auto& db = StaticDatabase::instance();
    std::cout << "[PASS] Singleton instance accessed.\n";

    // 2. Verify Base Stats (Flutter Mane)
    const auto& flutter = db.base_stats(SpeciesFlutterMane);
    std::cout << "Flutter Mane SpA: " << (int)flutter.spa << "\n";
    if (flutter.spa == 135 && flutter.spe == 135) {
        std::cout << "[PASS] Base Stats loaded correctly.\n";
    } else {
        std::cout << "[FAIL] Base Stats are wrong!\n";
        return 1;
    }

    // 3. Verify Move Data (Collision Course)
    const auto& cc = db.move_data(MoveCollisionCourse);
    if (cc.power == 100 && cc.type == TypeFighting && (cc.flags & MoveFlagContact)) {
        std::cout << "[PASS] Move Data loaded correctly.\n";
    } else {
        std::cout << "[FAIL] Move Data is wrong!\n";
        return 1;
    }

    // 4. Verify Type Chart
    // Fire vs Grass -> 2.0
    float fire_grass = db.type_multiplier(TypeFire, TypeGrass);
    // Ghost vs Normal -> 0.0
    float ghost_normal = db.type_multiplier(TypeGhost, TypeNormal);
    // Steel vs Fairy -> 2.0
    float steel_fairy = db.type_multiplier(TypeSteel, TypeFairy);

    if (is_close(fire_grass, 2.0f) && 
        is_close(ghost_normal, 0.0f) && 
        is_close(steel_fairy, 2.0f)) {
        std::cout << "[PASS] Type Chart logic is correct.\n";
    } else {
        std::cout << "[FAIL] Type Chart is broken! " 
                  << fire_grass << " " << ghost_normal << " " << steel_fairy << "\n";
        return 1;
    }

    // 5. Verify Random Sets
    const auto& koraidon_set = db.random_battle_set(SpeciesKoraidon);
    if (koraidon_set.moves.size() == 2 && koraidon_set.items[0] == ItemChoiceScarf) {
        std::cout << "[PASS] Random Sets loaded correctly.\n";
    } else {
        std::cout << "[FAIL] Random Sets are wrong.\n";
        return 1;
    }

    return 0;
}