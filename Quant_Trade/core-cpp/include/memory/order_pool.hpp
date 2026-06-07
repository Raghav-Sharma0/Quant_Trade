#pragma once

#include "../common/types.hpp"
#include "../common/constants.hpp"
#include "object_pool.hpp"

namespace hft {

// ============================================================================
// OrderPool — fixed-size pool of Order objects
// Capacity = MAX_ORDERS (from constants.hpp)
// ============================================================================
using OrderPool = ObjectPool<Order, MAX_ORDERS>;

} // namespace hft
