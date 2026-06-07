#pragma once

#include <cstdint>
#include "../common/types.hpp"
#include "../common/cacheline.hpp"

namespace hft {

// ============================================================================
// OrderNode
//
// Intrusive doubly-linked list node.
// No std::list, no heap allocation per node.
// Nodes live in a pool (OrderPool) — the pool owns the memory.
//
// Layout fits 2 nodes per 3 cache lines (node = 80 bytes).
// Hot fields (order_id, price, remaining_qty) sit at the front.
// ============================================================================
struct alignas(CACHELINE_SIZE) OrderNode {
    // ------ Intrusive list pointers (8+8 = 16 bytes) ------
    OrderNode* next = nullptr;
    OrderNode* prev = nullptr;

    // ------ Order identity (hot path reads) ------
    OrderId   order_id    = 0;    //  8
    Price     price       = 0;    //  4
    Quantity  orig_qty    = 0;    //  4  original submitted quantity
    Quantity  remain_qty  = 0;    //  4  remaining unfilled
    ClientId  client_id   = 0;    //  4
    Timestamp timestamp   = 0;    //  8  nanoseconds / RDTSC at submission
    SequenceNo sequence   = 0;    //  8  global sequence number
    SymbolId  symbol_id   = 0;    //  2
    uint8_t   side        = 0;    //  1  (OrderSide)
    uint8_t   type        = 0;    //  1  (OrderType)
    uint8_t   status      = 0;    //  1  (OrderStatus)
    uint8_t   _pad[3]     = {};   //  3  → running total = 16+8+4+4+4+4+8+8+2+1+1+1+3 = 64

    // -------------------------------------------------------------------------
    // Populate from an Order (gateway → node)
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // Write back to Order (for ExecutionReport generation)
    // -------------------------------------------------------------------------
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
