#pragma once


#include "../common/types.hpp"

namespace hft {

class Strategy {
protected:
    uint32_t spread_threshold;
    uint32_t order_qty;
    
public:
    Strategy() : spread_threshold(5), order_qty(100) {}
    virtual ~Strategy() = default;
    
    virtual Order* generate_order(const MarketData& md) = 0;
};

class SimpleSpreadStrategy : public Strategy {
private:
    uint64_t order_counter;
    uint32_t client_id;

public:
    SimpleSpreadStrategy(uint32_t cid = 1)
        : order_counter(0), client_id(cid) {
        spread_threshold = 5;
        order_qty = 100;
    }

    ~SimpleSpreadStrategy() override = default;

    Order* generate_order(const MarketData& md) override {
        uint32_t spread = md.best_ask_price - md.best_bid_price;

        if (spread < spread_threshold) {
            return nullptr;
        }

        Order* order = new Order();

        if (md.best_bid_price > 0) {
            order->order_id = ++order_counter;
            order->price = md.best_bid_price - 1;
            order->quantity = order_qty;
            order->side = static_cast<uint8_t>(OrderSide::BUY);
            order->symbol_id = md.symbol_id;
            order->client_id = client_id;
        }

        return order;
    }
};
} // namespace hft