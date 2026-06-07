#pragma once

#include "../common/types.hpp"
#include "../common/constants.hpp"
#include "object_pool.hpp"

namespace hft {

// ============================================================================
// TradePool — fixed-size pool of Trade objects
// Capacity = MAX_TRADES (from constants.hpp)
// ============================================================================
using TradePool = ObjectPool<Trade, MAX_TRADES>;

} // namespace hft
