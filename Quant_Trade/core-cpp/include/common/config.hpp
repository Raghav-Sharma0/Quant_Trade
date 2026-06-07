#pragma once

#include <cstdint>
#include <string>

namespace hft {

struct Config {
    uint16_t num_symbols = 1024;
    uint32_t max_orders_per_client = 10000;
    uint32_t risk_check_enabled = 1;
    uint32_t strategy_enabled = 1;
    double cpu_frequency_ghz = 3.0;
    
    std::string log_file = "hft_trading.log";
    std::string stats_file = "hft_stats.csv";
};

} // namespace hft