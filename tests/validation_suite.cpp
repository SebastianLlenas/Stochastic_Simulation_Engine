#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

#include "../src/data/database.h"
#include "../src/engine/action.h"
#include "../src/engine/damage.h"
#include "../src/engine/step.h"

using namespace omega9;

namespace {

constexpr uint8_t kPerfectIv = 31;
constexpr uint16_t kRandbatsEv = 85;

uint16_t calc_stat(uint16_t base, uint8_t level, uint16_t ev,
                   uint8_t iv = kPerfectIv) {
  if (level == 0) return 0;
  const uint32_t ev_quarter = ev / 4u;
  uint32_t value = (2u * base + iv + ev_quarter);
  value = (value * level) / 100u;
  value += 5u;
  return static_cast<uint16_t>(value);
}

uint16_t calc_hp(uint16_t base, uint8_t level, uint16_t ev,
                 uint8_t iv = kPerfectIv) {
  if (level == 0) return 0;
  const uint32_t ev_quarter = ev / 4u;
  uint32_t value = (2u * base + iv + ev_quarter);
  value = (value * level) / 100u;
  value += static_cast<uint32_t>(level) + 10u;
  return static_cast<uint16_t>(value);
}

uint16_t apply_stage(uint16_t stat, int stage) {
  if (stage >= 0) {
    const uint32_t num = static_cast<uint32_t>(2 + stage);
    return static_cast<uint16_t>((static_cast<uint32_t>(stat) * num) / 2u);
  }
  const uint32_t den = static_cast<uint32_t>(2 + (-stage));
  return static_cast<uint16_t>((static_cast<uint32_t>(stat) * 2u) / den);
}

uint32_t apply_fraction(uint32_t value, uint32_t num, uint32_t den) {
  if (den == 0) return 0;
  return static_cast<uint32_t>((static_cast<uint64_t>(value) * num) / den);
}

uint8_t derive_base_for_stat(uint16_t target, uint8_t level, uint16_t ev) {
  for (uint16_t base = 1; base <= 255; ++base) {
    if (calc_stat(base, level, ev) == target) {
      return static_cast<uint8_t>(base);
    }
  }
  return 0;
}

uint8_t derive_base_for_hp(uint16_t target, uint8_t level, uint16_t ev) {
  for (uint16_t base = 1; base <= 255; ++base) {
    if (calc_hp(base, level, ev) == target) {
      return static_cast<uint8_t>(base);
    }
  }
  return 0;
}

struct StaticDatabaseHack {
  std::vector<BaseStats> base_stats;
  std::vector<MoveData> move_data;
  std::vector<RandomBattleSet> random_sets;
  std::array<float, kTypeCount * kTypeCount> type_chart;
};

bool configure_validation_database() {
  auto &db = StaticDatabase::instance();
  auto &hack = *reinterpret_cast<StaticDatabaseHack *>(&db);

  // Desired Showdown stats for the scenario.
  constexpr uint8_t level = 80;
  const uint16_t zacian_atk = calc_stat(170, level, 0);
  const uint16_t kyogre_def = calc_stat(90, level, 0);
  const uint16_t kyogre_hp = calc_hp(100, level, 252);

  const uint8_t zacian_atk_base =
      derive_base_for_stat(zacian_atk, level, kRandbatsEv);
  const uint8_t kyogre_def_base =
      derive_base_for_stat(kyogre_def, level, kRandbatsEv);
  const uint8_t kyogre_hp_base =
      derive_base_for_hp(kyogre_hp, level, kRandbatsEv);

  if (zacian_atk_base == 0 || kyogre_def_base == 0 || kyogre_hp_base == 0) {
    return false;
  }

  BaseStats &zacian =
      const_cast<BaseStats &>(db.base_stats(SpeciesFlutterMane));
  zacian = BaseStats{92, zacian_atk_base, 115, 80, 115, 148, TypeFairy,
                     TypeSteel};

  BaseStats &kyogre =
      const_cast<BaseStats &>(db.base_stats(SpeciesKoraidon));
  kyogre = BaseStats{kyogre_hp_base, 100, kyogre_def_base, 150, 140, 90,
                     TypeWater, TypeNone};

  BaseStats &charizard =
      const_cast<BaseStats &>(db.base_stats(SpeciesIronHands));
  charizard =
      BaseStats{78, 84, 78, 109, 85, 100, TypeFire, TypeFlying};

  MoveData &behemoth_blade =
      const_cast<MoveData &>(db.move_data(MoveMoonblast));
  behemoth_blade = MoveData{100, TypeSteel, 100, 0, MovePhysical,
                            MoveFlagContact};

  MoveData &origin_pulse =
      const_cast<MoveData &>(db.move_data(MoveThunderPunch));
  origin_pulse = MoveData{90, TypeWater, 100, 0, MoveSpecial, 0};

  RandomBattleSet &zacian_set =
      const_cast<RandomBattleSet &>(db.random_battle_set(SpeciesFlutterMane));
  zacian_set.moves = {MoveMoonblast};
  zacian_set.items = {ItemNone};

  RandomBattleSet &kyogre_set =
      const_cast<RandomBattleSet &>(db.random_battle_set(SpeciesKoraidon));
  kyogre_set.moves = {MoveThunderPunch};
  kyogre_set.items = {ItemNone};

  RandomBattleSet &charizard_set =
      const_cast<RandomBattleSet &>(db.random_battle_set(SpeciesIronHands));
  charizard_set.moves = {MoveMoonblast};
  charizard_set.items = {ItemNone};

  const std::size_t steel_vs_water =
      static_cast<std::size_t>(TypeSteel) * kTypeCount + TypeWater;
  hack.type_chart[steel_vs_water] = 1.0f;

  return true;
}

std::array<uint16_t, 16> manual_behemoth_blade_rolls() {
  constexpr uint8_t level = 80;
  constexpr uint16_t power = 100;

  const uint16_t atk = calc_stat(170, level, 0);
  const uint16_t def = calc_stat(90, level, 0);
  const uint16_t boosted_atk = apply_stage(atk, 1);

  uint32_t damage = (2u * level) / 5u + 2u;
  uint64_t base = static_cast<uint64_t>(damage) * power * boosted_atk;
  base /= def;
  base /= 50u;
  base += 2u;
  damage = static_cast<uint32_t>(base);

  std::array<uint16_t, 16> rolls{};
  for (std::size_t i = 0; i < rolls.size(); ++i) {
    uint32_t roll = apply_fraction(damage, static_cast<uint32_t>(85u + i), 100u);
    // STAB (Steel) vs Water = neutral effectiveness.
    roll = apply_fraction(roll, 3u, 2u);
    rolls[i] = static_cast<uint16_t>(roll);
  }
  return rolls;
}

bool test_damage_accuracy() {
  BattleState state;
  std::memset(&state, 0, sizeof(BattleState));

  state.sides[0].active.species_id = SpeciesFlutterMane; // Zacian-Crowned
  state.sides[1].active.species_id = SpeciesKoraidon;    // Kyogre
  state.sides[0].active.level = 80;
  state.sides[1].active.level = 80;
  state.sides[0].active.hp = 1;
  state.sides[1].active.hp = 1;
  state.sides[0].active.stat_stages[StatAtk] = 1;

  const auto expected = manual_behemoth_blade_rolls();
  const DamageResult actual =
      calculate_damage(state, 0, MoveMoonblast, false);

  bool match = true;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (expected[i] != actual.rolls[i]) {
      match = false;
      break;
    }
  }

  if (match) {
    std::cout << "[PASS] Damage Accuracy Test\n";
  } else {
    std::cout << "[FAIL] Damage Accuracy Test\n";
    std::cout << "Expected rolls: ";
    for (auto v : expected) std::cout << v << " ";
    std::cout << "\nActual rolls:   ";
    for (auto v : actual.rolls) std::cout << v << " ";
    std::cout << "\n";
  }

  return match;
}

bool test_speed_tie() {
  BattleState state;
  std::memset(&state, 0, sizeof(BattleState));

  state.sides[0].active.species_id = SpeciesIronHands; // Charizard
  state.sides[1].active.species_id = SpeciesIronHands; // Charizard
  state.sides[0].active.level = 80;
  state.sides[1].active.level = 80;
  state.sides[0].active.hp = 100;
  state.sides[1].active.hp = 100;

  Action a1 = Action::make_move(0, MoveMoonblast, false);
  Action a2 = Action::make_move(1, MoveMoonblast, false);

  std::mt19937 rng(1337u);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  int p1_first = 0;
  for (int i = 0; i < 1000; ++i) {
    if (goes_first(state, a1, a2, dist(rng))) {
      ++p1_first;
    }
  }

  const bool pass = (p1_first >= 450 && p1_first <= 550);
  if (pass) {
    std::cout << "[PASS] Speed Tie Test (P1 first " << p1_first << "/1000)\n";
  } else {
    std::cout << "[FAIL] Speed Tie Test (P1 first " << p1_first << "/1000)\n";
  }

  return pass;
}

bool test_turn_order_integration() {
  BattleState state;
  std::memset(&state, 0, sizeof(BattleState));

  state.sides[0].active.species_id = SpeciesFlutterMane; // Zacian
  state.sides[1].active.species_id = SpeciesKoraidon;    // Kyogre
  state.sides[0].active.level = 80;
  state.sides[1].active.level = 80;
  state.sides[0].active.hp = 10; // Low HP
  state.sides[1].active.hp = 1; // Guaranteed KO
  state.sides[0].active.stat_stages[StatAtk] = 6; // Ensure OHKO

  Action p1 = Action::make_move(0, MoveMoonblast, false); // Behemoth Blade
  Action p2 = Action::make_move(1, MoveThunderPunch, false); // Origin Pulse

  auto result = step(state, p1, p2, 0.42f);
  const BattleState &next_state = result.first;

  const bool p2_fainted = next_state.sides[1].active.hp == 0;
  const bool p1_untouched = next_state.sides[0].active.hp == state.sides[0].active.hp;

  if (p2_fainted && p1_untouched) {
    std::cout << "[PASS] Turn Order Integration\n";
    return true;
  }

  std::cout << "[FAIL] Turn Order Integration\n";
  std::cout << "P2 HP: " << next_state.sides[1].active.hp
            << " | P1 HP: " << next_state.sides[0].active.hp << "\n";
  return false;
}

bool test_hazard_interaction() {
  BattleState state;
  std::memset(&state, 0, sizeof(BattleState));

  state.hazard_mask[0] |= HazardStealthRock;

  state.sides[0].active.species_id = SpeciesFlutterMane; // Zacian
  state.sides[0].active.level = 80;
  state.sides[0].active.hp = 100;

  state.sides[0].bench[0].species_id = SpeciesIronHands; // Charizard
  state.sides[0].bench[0].level = 80;
  state.sides[0].bench[0].hp_percent = 50;

  state.sides[1].active.species_id = SpeciesKoraidon; // Kyogre
  state.sides[1].active.level = 80;
  state.sides[1].active.hp = 100;

  Action p1 = Action::make_switch(0, 0);
  Action p2 = Action::make_move(1, MoveThunderPunch, false);

  auto result = step(state, p1, p2, 0.27f);
  const BattleState &next_state = result.first;

  const bool fainted = next_state.sides[0].active.hp == 0;
  if (fainted) {
    std::cout << "[PASS] Hazard Interaction\n";
  } else {
    std::cout << "[FAIL] Hazard Interaction (HP: "
              << next_state.sides[0].active.hp << ")\n";
  }

  return fainted;
}

} // namespace

int main() {
  std::cout << "=== Omega-9 Validation Suite ===\n";

  if (!configure_validation_database()) {
    std::cout << "[FAIL] Validation DB setup failed\n";
    return 1;
  }

  bool all_pass = true;
  all_pass &= test_damage_accuracy();
  all_pass &= test_speed_tie();
  all_pass &= test_turn_order_integration();
  all_pass &= test_hazard_interaction();

  return all_pass ? 0 : 1;
}
