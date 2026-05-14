#pragma once

#include <vector>

#include "core/battle_state.h"
#include "data/database.h"

namespace omega9 {

constexpr int kFeatureCount =
    5  /* weather one-hot */ + 5 /* terrain one-hot */ + 1 /* room */ +
    static_cast<int>(kSides) *
        (7 /* hazards */ +
         (1 /* active hp */ + 1 /* species */ +
          2 * (static_cast<int>(kTypeCount) + 1) /* types */ +
          static_cast<int>(StatCount) /* stat stages */ + 6 /* status */ +
          3 /* volatiles */) +
         static_cast<int>(kBenchSize) * (1 /* hp */ + 1 /* species */ +
                                          6 /* status */));

void encode_batch(const std::vector<BattleState> &states, float *out_buffer);

} // namespace omega9
