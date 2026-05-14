#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include "data/database.h"

namespace omega9 {

constexpr std::size_t kSides = 2;
constexpr std::size_t kTeamSize = 6;
constexpr std::size_t kBenchSize = kTeamSize - 1;

// Global field flags (weather, terrain, room effects).
enum GlobalFieldBits : uint64_t {
  FieldWeatherSun = 1ull << 0,
  FieldWeatherRain = 1ull << 1,
  FieldWeatherSand = 1ull << 2,
  FieldWeatherSnow = 1ull << 3,

  FieldTerrainElectric = 1ull << 8,
  FieldTerrainGrassy = 1ull << 9,
  FieldTerrainMisty = 1ull << 10,
  FieldTerrainPsychic = 1ull << 11,

  FieldRoomTrick = 1ull << 16,
};

// Hazards are stored per side in BattleState::hazard_mask.
enum HazardBits : uint64_t {
  HazardStealthRock = 1ull << 0,
  HazardSpikes1 = 1ull << 1,
  HazardSpikes2 = 1ull << 2,
  HazardSpikes3 = 1ull << 3,
  HazardToxicSpikes1 = 1ull << 4,
  HazardToxicSpikes2 = 1ull << 5,
  HazardStickyWeb = 1ull << 6,
};

// Side condition flags (per-side effects like Tailwind).
enum SideConditionBits : uint16_t {
  SideConditionTailwind = 1u << 0,
  SideConditionTeraUsed = 1u << 1,
  SideConditionReflect = 1u << 2,
  SideConditionLightScreen = 1u << 3,
  SideConditionAuroraVeil = 1u << 4,
};

// Major status conditions for active and bench Pokemon.
enum MajorStatusBits : uint8_t {
  StatusBurn = 1u << 0,
  StatusParalysis = 1u << 1,
  StatusSleep = 1u << 2,
  StatusPoison = 1u << 3,
  StatusToxic = 1u << 4,
  StatusFreeze = 1u << 5,
};

// Tera state flags for the active Pokemon.
enum TeraStateBits : uint8_t {
  TeraActive = 1u << 0,
  TeraUsed = 1u << 1,
};

// Volatile status bits (bit indices are explicit and stable).
enum VolatileBits : uint64_t {
  VolatileConfused = 1ull << 0,
  VolatileLeechSeed = 1ull << 1,
  VolatileEncore = 1ull << 2,
  VolatileFocusEnergy = 1ull << 3,
  VolatileTrapped = 1ull << 4,
  VolatileFlinch = 1ull << 5,
  VolatileProtect = 1ull << 6,
  VolatileFlying = 1ull << 7,
  VolatileDigging = 1ull << 8,
  VolatileSubstitute = 1ull << 9,
  VolatileEndure = 1ull << 10,
};

enum StatIndex : uint8_t {
  StatAtk = 0,
  StatDef = 1,
  StatSpa = 2,
  StatSpd = 3,
  StatSpe = 4,
  StatAcc = 5,
  StatEva = 6,
  StatCount = 7,
};

struct ActiveMon {
  // Volatiles are a 64-bit bitboard for fast masking.
  uint64_t volatile_mask = 0;

  uint16_t species_id = 0;
  uint8_t level = 0;
  uint16_t hp = 0;  // Current HP (integer)
  uint16_t substitute_hp = 0;  // Current Substitute HP (0 = none)
  uint16_t item_id = 0;  // Held item (ItemID)
  uint16_t choice_move_id = 0;  // Move locked by Choice item (0 = none)

  std::array<MoveID, 4> moves{};
  std::array<uint8_t, 4> pp{};

  // Stat stages in range [-6, +6] for each StatIndex.
  std::array<int8_t, StatCount> stat_stages{};

  uint8_t major_status = 0;   // MajorStatusBits
  uint8_t sleep_turns = 0;    // Remaining sleep turns if StatusSleep is set
  uint8_t toxic_turns = 0;    // Toxic counter for StatusToxic
  uint8_t tera_type = 0;      // Type id (0..17)
  uint8_t tera_state = 0;     // TeraStateBits

  // Common volatile turn counters.
  uint8_t confusion_turns = 0;
  uint8_t encore_turns = 0;
  uint8_t protect_counter = 0;
  std::array<uint8_t, 1> reserved{};  // Padding / future counters
};

struct BenchMon {
  uint16_t species_id = 0;
  uint8_t level = 0;
  uint8_t hp_percent = 0;  // 0..100
  uint8_t status_mask = 0; // MajorStatusBits
  uint8_t sleep_turns = 0; // Remaining sleep turns if StatusSleep is set
  uint8_t tera_type = 0;   // Type id (0..17)
  uint8_t tera_state = 0;  // TeraStateBits
  uint16_t item_id = 0;    // Held item (ItemID)
  std::array<MoveID, 4> moves{};
  std::array<uint8_t, 4> pp{};
};

struct SideState {
  ActiveMon active{};
  uint16_t side_condition_mask = 0;  // SideConditionBits
  uint8_t tailwind_turns = 0;
  uint8_t reflect_turns = 0;
  uint8_t light_screen_turns = 0;
  uint8_t aurora_veil_turns = 0;
  std::array<BenchMon, kBenchSize> bench{};
};

struct alignas(64) BattleState {
  uint64_t global_field_mask = 0;
  std::array<uint64_t, kSides> hazard_mask{};  // HazardBits per side
  uint8_t force_switch_mask = 0;  // bit 0: P1 forced, bit 1: P2 forced
  uint8_t weather_turns = 0;
  uint8_t terrain_turns = 0;
  uint8_t trick_room_turns = 0;

  std::array<SideState, kSides> sides{};
};

static_assert(std::is_trivially_copyable_v<BattleState>,
              "BattleState must be trivially copyable.");
static_assert(sizeof(BattleState) <= 4096, "BattleState exceeds 4KB.");

} // namespace omega9
