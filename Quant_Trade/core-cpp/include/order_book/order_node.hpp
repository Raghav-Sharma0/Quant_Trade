#pragma once

#include <cstdint>
#include "../common/types.hpp"
#include "../common/cacheline.hpp"

namespace hft {

struct alignas(CACHELINE_SIZE) OrderNode {
    OrderNode* next = nullptr;
    OrderNode* prev = nullptr;

    OrderId   order_id    = 0;
    Price     price       = 0;
    Quantity  orig_qty    = 0;
    Quantity  remain_qty  = 0;
    ClientId  client_id   = 0;
    Timestamp timestamp   = 0;
    SequenceNo sequence   = 0;
    SymbolId  symbol_id   = 0;
    uint8_t   side        = 0;
    uint8_t   type        = 0;
    uint8_t   status      = 0;
    uint8_t   _pad[3]     = {};

    void from_order(const Order& o) noexcept {
        order_id   = o.order_id;
        price      = o.price;
        orig_qty   = o.quantity;
        remain_qty = o.quantity - o.filled_qty;
        client_id  = o.client_id;
        timestamp  = o.timestamp;
        sequence   = o.sequence;
        symbol_id  = o.symbol_id;
        side       = o.side;
        type       = o.type;
        status     = o.status;
        next = prev = nullptr;
    }

    void to_order(Order& o) const noexcept {
        o.order_id   = order_id;
        o.price      = price;
        o.quantity   = orig_qty;
        o.filled_qty = orig_qty - remain_qty;
        o.client_id  = client_id;
        o.timestamp  = timestamp;
        o.sequence   = sequence;
        o.symbol_id  = symbol_id;
        o.side       = side;
        o.type       = type;
        o.status     = status;
    }

    [[nodiscard]] bool is_buy()    const noexcept { return side == static_cast<uint8_t>(OrderSide::BUY); }
    [[nodiscard]] bool is_filled() const noexcept { return remain_qty == 0; }
    [[nodiscard]] bool is_market() const noexcept { return type == static_cast<uint8_t>(OrderType::MARKET); }
};

static_assert(sizeof(OrderNode) == 64, "OrderNode must be exactly 64 bytes");

} // namespace hft
