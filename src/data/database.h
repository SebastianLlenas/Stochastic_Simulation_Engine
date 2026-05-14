#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace omega9 {

using SpeciesID = uint16_t;
using MoveID = uint16_t;
using ItemID = uint16_t;
using TypeID = uint8_t;

constexpr std::size_t kTypeCount = 18;

enum TypeIndex : uint8_t {
  TypeNormal = 0,
  TypeFire = 1,
  TypeWater = 2,
  TypeElectric = 3,
  TypeGrass = 4,
  TypeIce = 5,
  TypeFighting = 6,
  TypePoison = 7,
  TypeGround = 8,
  TypeFlying = 9,
  TypePsychic = 10,
  TypeBug = 11,
  TypeRock = 12,
  TypeGhost = 13,
  TypeDragon = 14,
  TypeDark = 15,
  TypeSteel = 16,
  TypeFairy = 17,
  TypeNone = 255,
};

enum MoveCategory : uint8_t {
  MovePhysical = 0,
  MoveSpecial = 1,
  MoveStatus = 2,
};

enum MoveFlags : uint16_t {
  MoveFlagContact = 1u << 0,
  MoveFlagPunch = 1u << 1,
  MoveFlagSound = 1u << 2,
  MoveFlagBypassProtect = 1u << 4,
  MoveFlagHitsFlying = 1u << 5,
  MoveFlagHitsDigging = 1u << 6,
  MoveFlagIgnoreImmunity = 1u << 7,
  MoveFlagDefrost = 1u << 8,
  MoveFlagTargetSelf = 1u << 9,
  MoveFlagBypassSubstitute = 1u << 10,
};

enum SpeciesIndex : SpeciesID {
  SpeciesNone = 0,
  SpeciesFlutterMane = 1,
  SpeciesKoraidon = 2,
  SpeciesIronHands = 3,
  SpeciesCount = 4,
};

enum MoveIndex : MoveID {
  MoveNone = 0,
  MoveMoonblast = 1,
  MoveShadowBall = 2,
  MoveCollisionCourse = 3,
  MoveDragonClaw = 4,
  MoveThunderPunch = 5,
  MoveDrainPunch = 6,
  MovePsyshock = 7,
  MoveBodyPress = 8,
  MoveFoulPlay = 9,
  MoveEarthquake = 10,
  MoveBulldoze = 11,
  MoveStruggle = 12,
  MoveProtect = 13,
  MoveCount = 14,
};

enum ItemIndex : ItemID {
  ItemNone = 0,
  ItemBoosterEnergy = 1,
  ItemChoiceScarf = 2,
  ItemLeftovers = 3,
  ItemHeavyDutyBoots = 4,
  ItemChoiceBand = 5,
  ItemChoiceSpecs = 6,
  ItemAssaultVest = 7,
  ItemEviolite = 8,
  ItemLifeOrb = 9,
  ItemFocusSash = 10,
  ItemCount = 11,
};

struct BaseStats {
  uint8_t hp = 0;
  uint8_t atk = 0;
  uint8_t def = 0;
  uint8_t spa = 0;
  uint8_t spd = 0;
  uint8_t spe = 0;
  uint8_t type1 = TypeNormal;
  uint8_t type2 = TypeNone;
};

struct MoveData {
  uint8_t power = 0;
  uint8_t type = 0;
  uint8_t accuracy = 0;
  int8_t priority = 0;
  uint8_t category = 0;
  uint16_t flags = 0;
  uint8_t primary_status = 0;
  uint8_t crit_ratio = 0;
  uint8_t min_hits = 1;
  uint8_t max_hits = 1;
  uint8_t pp = 0;
};

struct RandomBattleSet {
  std::vector<MoveID> moves;
  std::vector<ItemID> items;
};

class StaticDatabase {
public:
  static StaticDatabase &instance();

  void load_dummy_data();

  const BaseStats &base_stats(SpeciesID id) const;
  const MoveData &move_data(MoveID id) const;
  const RandomBattleSet &random_battle_set(SpeciesID id) const;
  void fill_moveset(SpeciesID id, std::array<MoveID, 4> &moves,
                    std::array<uint8_t, 4> &pp) const;
  float type_multiplier(TypeID attacker, TypeID defender) const;

  std::size_t species_count() const { return base_stats_.size(); }
  std::size_t move_count() const { return move_data_.size(); }

private:
  StaticDatabase();
  void init_type_chart();

  std::vector<BaseStats> base_stats_;
  std::vector<MoveData> move_data_;
  std::vector<RandomBattleSet> random_sets_;
  std::array<float, kTypeCount * kTypeCount> type_chart_{};
};

} // namespace omega9
