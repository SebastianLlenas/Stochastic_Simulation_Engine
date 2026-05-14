#include "engine/switch.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "data/database.h"
#include "engine/mutator.h"
#include "engine/utils.h"

namespace omega9 {
namespace {

constexpr uint8_t kPerfectIv = 31;
constexpr uint8_t kRandbatsEv = 85;

uint16_t max_hp_value(const BaseStats &base, uint8_t level) {
  if (level == 0) return 0;
  const uint32_t ev = kRandbatsEv / 4u;
  uint32_t value = (2u * base.hp + kPerfectIv + ev);
  value = (value * level) / 100u;
  value += static_cast<uint32_t>(level) + 10u;
  return static_cast<uint16_t>(
      std::min<uint32_t>(value, std::numeric_limits<uint16_t>::max()));
}

uint8_t hp_percent_from_hp(uint16_t hp, uint16_t max_hp) {
  if (max_hp == 0 || hp == 0) return 0;
  uint32_t percent =
      (static_cast<uint32_t>(hp) * 100u + max_hp - 1u) / max_hp;
  if (percent > 100u) percent = 100u;
  return static_cast<uint8_t>(percent);
}

uint16_t hp_from_percent(uint8_t percent, uint16_t max_hp) {
  if (max_hp == 0 || percent == 0) return 0;
  uint32_t hp = (static_cast<uint32_t>(max_hp) * percent) / 100u;
  if (hp == 0) hp = 1;
  if (hp > max_hp) hp = max_hp;
  return static_cast<uint16_t>(hp);
}

bool has_type(const ActiveMon &active, const BaseStats &base, TypeID type) {
  if (active.tera_state & TeraActive) {
    return active.tera_type == type;
  }
  if (base.type1 == type) return true;
  return base.type2 < kTypeCount && base.type2 == type;
}

void apply_type_multiplier(const StaticDatabase &db, TypeID move_type,
                           TypeID defender_type, uint32_t &num, uint32_t &den,
                           bool &zero_out) {
  if (defender_type >= kTypeCount || zero_out) return;
  const float mult = db.type_multiplier(move_type, defender_type);
  if (mult == 0.0f) {
    zero_out = true;
    return;
  }
  if (mult == 0.5f) {
    den *= 2u;
  } else if (mult == 2.0f) {
    num *= 2u;
  }
}

uint16_t fraction_damage(uint16_t max_hp, uint32_t num, uint32_t den) {
  if (max_hp == 0 || num == 0 || den == 0) return 0;
  const uint64_t raw =
      (static_cast<uint64_t>(max_hp) * static_cast<uint64_t>(num)) /
      static_cast<uint64_t>(den);
  const uint64_t clamped = (raw == 0) ? 1u : raw;
  return static_cast<uint16_t>(std::min<uint64_t>(
      clamped, static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())));
}

bool has_heavy_duty_boots(const ActiveMon &active) {
  return active.item_id == ItemHeavyDutyBoots;
}

bool has_magic_guard(const ActiveMon &) {
  return false;
}

int spikes_layers(uint64_t hazard_mask) {
  if (hazard_mask & HazardSpikes3) return 3;
  if (hazard_mask & HazardSpikes2) return 2;
  if (hazard_mask & HazardSpikes1) return 1;
  return 0;
}

int toxic_spikes_layers(uint64_t hazard_mask) {
  if (hazard_mask & HazardToxicSpikes2) return 2;
  if (hazard_mask & HazardToxicSpikes1) return 1;
  return 0;
}

} // namespace

void execute_switch(BattleState &state, uint8_t side, BenchSlotIndex slot) {
  if (side >= kSides || slot >= kBenchSize) return;

  SideState &side_state = state.sides[side];
  ActiveMon &active = side_state.active;
  BenchMon &bench = side_state.bench[slot];

  if (bench.species_id == 0) return;

  const auto &db = StaticDatabase::instance();
  if (bench.species_id >= db.species_count()) return;

  uint16_t active_max_hp = 0;
  if (active.species_id != 0 && active.species_id < db.species_count()) {
    active_max_hp =
        max_hp_value(db.base_stats(active.species_id), active.level);
  }
  const uint16_t bench_max_hp =
      max_hp_value(db.base_stats(bench.species_id), bench.level);

  const uint8_t active_percent = hp_percent_from_hp(active.hp, active_max_hp);
  const uint16_t incoming_hp = hp_from_percent(bench.hp_percent, bench_max_hp);

  BenchMon outgoing{};
  outgoing.species_id = active.species_id;
  outgoing.level = active.level;
  outgoing.hp_percent = active_percent;
  outgoing.status_mask = active.major_status;
  outgoing.sleep_turns = active.sleep_turns;
  outgoing.tera_type = active.tera_type;
  outgoing.tera_state = active.tera_state;
  outgoing.item_id = active.item_id;
  outgoing.moves = active.moves;
  outgoing.pp = active.pp;

  active.species_id = bench.species_id;
  active.level = bench.level;
  active.hp = incoming_hp;
  active.item_id = bench.item_id;
  active.choice_move_id = 0;
  active.major_status = bench.status_mask;
  active.sleep_turns = bench.sleep_turns;
  active.toxic_turns = 0;
  active.tera_type = bench.tera_type;
  active.tera_state = bench.tera_state;
  active.moves = bench.moves;
  active.pp = bench.pp;

  active.volatile_mask = 0;
  active.stat_stages.fill(0);
  active.confusion_turns = 0;
  active.encore_turns = 0;
  active.protect_counter = 0;
  active.reserved.fill(0);

  bench = outgoing;
}

void apply_entry_hazards(BattleState &state, uint8_t side) {
  if (side >= kSides) return;

  SideState &side_state = state.sides[side];
  ActiveMon &active = side_state.active;

  if (active.species_id == 0) return;

  const auto &db = StaticDatabase::instance();
  if (active.species_id >= db.species_count()) return;

  if (has_heavy_duty_boots(active)) return;

  const BaseStats &base = db.base_stats(active.species_id);
  uint64_t &hazards = state.hazard_mask[side];

  const bool magic_guard = has_magic_guard(active);
  const bool grounded = is_grounded(active, base);

  const bool has_damage_hazards =
      (hazards & HazardStealthRock) ||
      (hazards & (HazardSpikes1 | HazardSpikes2 | HazardSpikes3));
  uint16_t max_hp = 0;
  if (has_damage_hazards && !magic_guard) {
    max_hp = max_hp_value(base, active.level);
  }

  if ((hazards & HazardStealthRock) && !magic_guard) {
    uint32_t type_num = 1u;
    uint32_t type_den = 1u;
    bool zero_out = false;
    if (active.tera_state & TeraActive) {
      apply_type_multiplier(db, TypeRock, active.tera_type, type_num, type_den,
                            zero_out);
    } else {
      apply_type_multiplier(db, TypeRock, base.type1, type_num, type_den,
                            zero_out);
      if (base.type2 < kTypeCount) {
        apply_type_multiplier(db, TypeRock, base.type2, type_num, type_den,
                              zero_out);
      }
    }
    if (!zero_out) {
      const uint16_t damage =
          fraction_damage(max_hp, type_num, 8u * type_den);
      StateMutator::apply_damage(state, side, damage, false, false);
      if (active.hp == 0) return;
    }
  }

  const int layers = spikes_layers(hazards);
  if (layers > 0 && grounded && !magic_guard) {
    uint32_t num = 1u;
    uint32_t den = 8u;
    if (layers == 2) {
      den = 6u;
    } else if (layers >= 3) {
      den = 4u;
    }
    const uint16_t damage = fraction_damage(max_hp, num, den);
    StateMutator::apply_damage(state, side, damage, false, false);
    if (active.hp == 0) return;
  }

  const int toxic_layers = toxic_spikes_layers(hazards);
  if (toxic_layers > 0) {
    if (grounded) {
      const bool poison_type = has_type(active, base, TypePoison);
      if (poison_type) {
        hazards &= ~(HazardToxicSpikes1 | HazardToxicSpikes2);
      } else {
        const bool steel_type = has_type(active, base, TypeSteel);
        if (!steel_type) {
          const uint8_t status =
              (toxic_layers >= 2) ? StatusToxic : StatusPoison;
          StateMutator::apply_status(state, side, status, 0.0f);
        }
      }
    }
  }

  if ((hazards & HazardStickyWeb) && grounded) {
    StateMutator::apply_boost(state, side, StatSpe, -1);
  }
}

Action get_switch_action(const BattleState &state, uint8_t side) {
  Action fallback{};
  fallback.side = side;

  if (side >= kSides) return fallback;
  const SideState &side_state = state.sides[side];

  for (BenchSlotIndex slot = 0; slot < kBenchSize; ++slot) {
    const BenchMon &bench = side_state.bench[slot];
    if (bench.species_id == 0) continue;
    if (bench.hp_percent == 0) continue;
    return Action::make_switch(side, slot);
  }

  return fallback;
}

} // namespace omega9
