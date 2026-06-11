#pragma once

#include "../common/types.hpp"
#include "../common/constants.hpp"
#include "object_pool.hpp"

namespace hft {

using OrderPool = ObjectPool<Order, MAX_ORDERS>;

} // namespace hft
