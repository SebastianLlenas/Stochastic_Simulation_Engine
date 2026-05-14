#include "engine/encoder.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "data/database.h"

namespace omega9 {
namespace {

constexpr uint8_t kPerfectIv = 31;
constexpr uint8_t kRandbatsEv = 85;

constexpr int kTypeFeatureCount = static_cast<int>(kTypeCount) + 1;

float clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

float normalize_stage(int8_t stage) {
  if (stage < -6) stage = -6;
  if (stage > 6) stage = 6;
  return (static_cast<float>(stage) + 6.0f) / 12.0f;
}

float normalize_species_id(uint16_t id, uint16_t max_id) {
  if (max_id == 0) return 0.0f;
  if (id > max_id) id = max_id;
  return static_cast<float>(id) / static_cast<float>(max_id);
}

uint16_t max_hp_value(const BaseStats &base, uint8_t level) {
  if (level == 0) return 0;
  const uint32_t ev = kRandbatsEv / 4u;
  uint32_t value = (2u * base.hp + kPerfectIv + ev);
  value = (value * level) / 100u;
  value += static_cast<uint32_t>(level) + 10u;
  return static_cast<uint16_t>(
      std::min<uint32_t>(value, std::numeric_limits<uint16_t>::max()));
}

float hp_ratio(uint16_t hp, uint16_t max_hp) {
  if (max_hp == 0 || hp == 0) return 0.0f;
  return clamp01(static_cast<float>(hp) / static_cast<float>(max_hp));
}

void write_type_one_hot(float *&cursor, TypeID type_id) {
  int index = static_cast<int>(kTypeCount);
  if (type_id < kTypeCount) {
    index = static_cast<int>(type_id);
  }
  for (int i = 0; i < kTypeFeatureCount; ++i) {
    *cursor++ = (i == index) ? 1.0f : 0.0f;
  }
}

} // namespace

void encode_batch(const std::vector<BattleState> &states, float *out_buffer) {
  if (states.empty() || out_buffer == nullptr) return;

  const auto &db = StaticDatabase::instance();
  const std::size_t species_count = db.species_count();
  const uint16_t max_species_id =
      species_count > 0 ? static_cast<uint16_t>(species_count - 1) : 0;

  const BattleState *state_data = states.data();
  const std::size_t count = states.size();

#pragma omp parallel for
  for (std::int64_t i = 0; i < static_cast<std::int64_t>(count); ++i) {
    const std::size_t index = static_cast<std::size_t>(i);
    const BattleState &state = state_data[index];
    float *cursor = out_buffer + index * kFeatureCount;

    const uint64_t field = state.global_field_mask;
    const bool has_weather =
        (field & (FieldWeatherSun | FieldWeatherRain | FieldWeatherSand |
                  FieldWeatherSnow)) != 0;
    *cursor++ = has_weather ? 0.0f : 1.0f;
    *cursor++ = (field & FieldWeatherSun) ? 1.0f : 0.0f;
    *cursor++ = (field & FieldWeatherRain) ? 1.0f : 0.0f;
    *cursor++ = (field & FieldWeatherSand) ? 1.0f : 0.0f;
    *cursor++ = (field & FieldWeatherSnow) ? 1.0f : 0.0f;

    const bool has_terrain =
        (field & (FieldTerrainElectric | FieldTerrainGrassy | FieldTerrainMisty |
                  FieldTerrainPsychic)) != 0;
    *cursor++ = has_terrain ? 0.0f : 1.0f;
    *cursor++ = (field & FieldTerrainElectric) ? 1.0f : 0.0f;
    *cursor++ = (field & FieldTerrainGrassy) ? 1.0f : 0.0f;
    *cursor++ = (field & FieldTerrainMisty) ? 1.0f : 0.0f;
    *cursor++ = (field & FieldTerrainPsychic) ? 1.0f : 0.0f;

    *cursor++ = (field & FieldRoomTrick) ? 1.0f : 0.0f;

    for (std::size_t side = 0; side < kSides; ++side) {
      const uint64_t hazards = state.hazard_mask[side];
      *cursor++ = (hazards & HazardStealthRock) ? 1.0f : 0.0f;
      *cursor++ = (hazards & HazardSpikes1) ? 1.0f : 0.0f;
      *cursor++ = (hazards & HazardSpikes2) ? 1.0f : 0.0f;
      *cursor++ = (hazards & HazardSpikes3) ? 1.0f : 0.0f;
      *cursor++ = (hazards & HazardToxicSpikes1) ? 1.0f : 0.0f;
      *cursor++ = (hazards & HazardToxicSpikes2) ? 1.0f : 0.0f;
      *cursor++ = (hazards & HazardStickyWeb) ? 1.0f : 0.0f;

      const SideState &side_state = state.sides[side];
      const ActiveMon &active = side_state.active;

      uint16_t max_hp = 0;
      const bool active_valid =
          active.species_id != 0 && active.species_id < species_count;
      if (active_valid) {
        max_hp = max_hp_value(db.base_stats(active.species_id), active.level);
      }
      *cursor++ = hp_ratio(active.hp, max_hp);
      *cursor++ = normalize_species_id(active.species_id, max_species_id);

      TypeID type1 = TypeNone;
      TypeID type2 = TypeNone;
      if (active_valid) {
        const BaseStats &base = db.base_stats(active.species_id);
        if (active.tera_state & TeraActive) {
          type1 = active.tera_type < kTypeCount ? active.tera_type : TypeNone;
          type2 = TypeNone;
        } else {
          type1 = base.type1 < kTypeCount ? base.type1 : TypeNone;
          type2 = base.type2 < kTypeCount ? base.type2 : TypeNone;
        }
      }
      write_type_one_hot(cursor, type1);
      write_type_one_hot(cursor, type2);

      for (std::size_t stat = 0; stat < StatCount; ++stat) {
        *cursor++ = normalize_stage(active.stat_stages[stat]);
      }

      const uint8_t status = active.major_status;
      *cursor++ = (status & StatusBurn) ? 1.0f : 0.0f;
      *cursor++ = (status & StatusParalysis) ? 1.0f : 0.0f;
      *cursor++ = (status & StatusSleep) ? 1.0f : 0.0f;
      *cursor++ = (status & StatusPoison) ? 1.0f : 0.0f;
      *cursor++ = (status & StatusToxic) ? 1.0f : 0.0f;
      *cursor++ = (status & StatusFreeze) ? 1.0f : 0.0f;

      const uint64_t volatiles = active.volatile_mask;
      *cursor++ = (volatiles & VolatileConfused) ? 1.0f : 0.0f;
      *cursor++ = (volatiles & VolatileLeechSeed) ? 1.0f : 0.0f;
      *cursor++ = (volatiles & VolatileEncore) ? 1.0f : 0.0f;

      for (const BenchMon &bench : side_state.bench) {
        *cursor++ = clamp01(static_cast<float>(bench.hp_percent) / 100.0f);
        *cursor++ = normalize_species_id(bench.species_id, max_species_id);

        const uint8_t bench_status = bench.status_mask;
        *cursor++ = (bench_status & StatusBurn) ? 1.0f : 0.0f;
        *cursor++ = (bench_status & StatusParalysis) ? 1.0f : 0.0f;
        *cursor++ = (bench_status & StatusSleep) ? 1.0f : 0.0f;
        *cursor++ = (bench_status & StatusPoison) ? 1.0f : 0.0f;
        *cursor++ = (bench_status & StatusToxic) ? 1.0f : 0.0f;
        *cursor++ = (bench_status & StatusFreeze) ? 1.0f : 0.0f;
      }
    }
  }
}

} // namespace omega9
