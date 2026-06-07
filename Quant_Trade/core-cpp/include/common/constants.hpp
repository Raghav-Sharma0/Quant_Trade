#pragma once

#include <cstdint>

namespace hft {

constexpr uint32_t MIN_PRICE = 1;
constexpr uint32_t MAX_PRICE = 1000000;
constexpr uint32_t MIN_ORDER_QTY = 1;
constexpr uint32_t MAX_ORDER_QTY = 10000000;

constexpr uint32_t DEFAULT_MAX_POSITION = 1000000;
constexpr int64_t DEFAULT_MAX_LOSS = 100000;

constexpr size_t LOGGER_BUFFER_SIZE = 65536;

} // namespace hft