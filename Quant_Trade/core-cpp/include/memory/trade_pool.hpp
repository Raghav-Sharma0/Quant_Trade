#pragma once

#include "../common/types.hpp"
#include "../common/constants.hpp"
#include "object_pool.hpp"

namespace hft {

using TradePool = ObjectPool<Trade, MAX_TRADES>;

} // namespace hft
