#pragma once

#include "../common/types.hpp"

namespace hft {

class FeedHandler {
public:
    FeedHandler() = default;
    ~FeedHandler() = default;
    
    void process_market_data(const MarketData& md) noexcept {
    }
};

} // namespace hft