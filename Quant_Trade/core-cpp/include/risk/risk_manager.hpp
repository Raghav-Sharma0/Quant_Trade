#pragma once

#include "../common/types.hpp"
#include <vector>

namespace hft {

class RiskManager {
private:
    struct ClientLimits {
        int64_t max_position;
        int64_t current_position;
        int64_t max_loss;
        int64_t current_loss;
        uint32_t max_order_qty;
    };
    
    std::vector<ClientLimits> limits;
    
public:
    enum class RiskCheckResult : uint8_t {
        APPROVED = 0,
        EXCEEDS_POSITION = 1,
        EXCEEDS_LOSS = 2,
        EXCEEDS_ORDER_QTY = 3
    };
    
    RiskManager() : limits(10000) {
        for (auto& l : limits) {
            l.max_position = 1000000;
            l.current_position = 0;
            l.max_loss = 100000;
            l.current_loss = 0;
            l.max_order_qty = 10000;
        }
    }
    
    ~RiskManager() = default;
    
    RiskCheckResult check_order(const Order& order) const noexcept {
        if (order.client_id >= limits.size()) {
            return RiskCheckResult::APPROVED;
        }
        
        const auto& client_limits = limits[order.client_id];
        
        if (order.quantity > client_limits.max_order_qty) {
            return RiskCheckResult::EXCEEDS_ORDER_QTY;
        }
        
        int64_t new_position = client_limits.current_position;
        if (order.side == static_cast<uint8_t>(OrderSide::BUY)) {
            new_position += order.quantity;
        } else {
            new_position -= order.quantity;
        }
        
        if (new_position > client_limits.max_position || 
            new_position < -client_limits.max_position) {
            return RiskCheckResult::EXCEEDS_POSITION;
        }
        
        if (client_limits.current_loss >= client_limits.max_loss) {
            return RiskCheckResult::EXCEEDS_LOSS;
        }
        
        return RiskCheckResult::APPROVED;
    }
    
    void on_trade(uint32_t client_id, const Trade& trade) noexcept {
        if (client_id >= limits.size()) return;
    }
    
    void set_limit(uint32_t client_id, int64_t max_pos, int64_t max_loss) noexcept {
        if (client_id < limits.size()) {
            limits[client_id].max_position = max_pos;
            limits[client_id].max_loss = max_loss;
        }
    }
};

} // namespace hft