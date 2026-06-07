#pragma once

#include <cstdint>
#include <cstring>
#include <array>

namespace hft {

// ============================================================================
// Fundamental scalar aliases
// ============================================================================
using OrderId    = uint64_t;
using TradeId    = uint64_t;
using Price      = uint32_t;   // Fixed-point: price in ticks (e.g. price * 100)
using Quantity   = uint32_t;
using SymbolId   = uint16_t;
using ClientId   = uint32_t;
using Timestamp  = uint64_t;   // RDTSC or nanoseconds since epoch
using SequenceNo = uint64_t;

// ============================================================================
// Enumerations (compact width)
// ============================================================================
enum class OrderSide : uint8_t {
    BUY  = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    LIMIT  = 0,
    MARKET = 1,
    IOC    = 2,   // Immediate-Or-Cancel
    FOK    = 3    // Fill-Or-Kill
};

enum class OrderStatus : uint8_t {
    PENDING          = 0,
    ACCEPTED         = 1,
    PARTIALLY_FILLED = 2,
    FILLED           = 3,
    CANCELLED        = 4,
    REJECTED         = 5,
    EXPIRED          = 6
};

enum class RejectReason : uint8_t {
    NONE               = 0,
    RISK_MAX_ORDER_QTY = 1,
    RISK_MAX_POSITION  = 2,
    RISK_MAX_LOSS      = 3,
    RISK_KILL_SWITCH   = 4,
    RISK_PRICE_COLLAR  = 5,
    UNKNOWN_SYMBOL     = 6,
    INVALID_PRICE      = 7,
    INVALID_QTY        = 8
};

// ============================================================================
// Order  (hot-path struct — fits in a single 64-byte cache line)
// ============================================================================
struct alignas(64) Order {
    OrderId   order_id;      //  8 bytes
    Price     price;         //  4 bytes
    Quantity  quantity;      //  4 bytes
    Quantity  filled_qty;    //  4 bytes
    ClientId  client_id;     //  4 bytes
    Timestamp timestamp;     //  8 bytes
    SequenceNo sequence;     //  8 bytes
    SymbolId  symbol_id;     //  2 bytes
    uint8_t   side;          //  1 byte  (OrderSide)
    uint8_t   type;          //  1 byte  (OrderType)
    uint8_t   status;        //  1 byte  (OrderStatus)
    uint8_t   reject_reason; //  1 byte  (RejectReason)
    uint8_t   _pad[14];      // 14 bytes  → total = 64 bytes

    Order() noexcept { std::memset(this, 0, sizeof(Order)); }

    Order(OrderId oid, Price p, Quantity q, OrderSide s, OrderType t,
          SymbolId sym, ClientId cid, Timestamp ts = 0) noexcept
        : order_id(oid), price(p), quantity(q), filled_qty(0),
          client_id(cid), timestamp(ts), sequence(0), symbol_id(sym),
          side(static_cast<uint8_t>(s)), type(static_cast<uint8_t>(t)),
          status(static_cast<uint8_t>(OrderStatus::PENDING)),
          reject_reason(static_cast<uint8_t>(RejectReason::NONE))
    {
        std::memset(_pad, 0, sizeof(_pad));
    }

    [[nodiscard]] Quantity remaining() const noexcept { return quantity - filled_qty; }
    [[nodiscard]] bool     is_buy()    const noexcept { return side == static_cast<uint8_t>(OrderSide::BUY); }
    [[nodiscard]] bool     is_filled() const noexcept { return filled_qty >= quantity; }
    [[nodiscard]] bool     is_market() const noexcept { return type == static_cast<uint8_t>(OrderType::MARKET); }
    [[nodiscard]] bool     is_ioc()    const noexcept { return type == static_cast<uint8_t>(OrderType::IOC); }
    [[nodiscard]] bool     is_fok()    const noexcept { return type == static_cast<uint8_t>(OrderType::FOK); }
};
static_assert(sizeof(Order) == 64, "Order must be exactly 64 bytes");

// ============================================================================
// Trade  (generated on each fill — also 64-byte aligned)
// ============================================================================
struct alignas(64) Trade {
    TradeId   trade_id;      //  8
    OrderId   bid_order_id;  //  8
    OrderId   ask_order_id;  //  8
    Price     price;         //  4
    Quantity  quantity;      //  4
    Timestamp timestamp;     //  8
    SequenceNo sequence;     //  8
    SymbolId  symbol_id;     //  2
    uint8_t   _pad[14];      // 14 → total = 64

    Trade() noexcept { std::memset(this, 0, sizeof(Trade)); }

    Trade(TradeId tid, OrderId bid, OrderId ask, Price p, Quantity q,
          SymbolId sym, Timestamp ts = 0) noexcept
        : trade_id(tid), bid_order_id(bid), ask_order_id(ask),
          price(p), quantity(q), timestamp(ts), sequence(0), symbol_id(sym)
    {
        std::memset(_pad, 0, sizeof(_pad));
    }
};
static_assert(sizeof(Trade) == 64, "Trade must be exactly 64 bytes");

// ============================================================================
// ExecutionReport  (sent to clients / risk / recorder)
// ============================================================================
struct alignas(64) ExecutionReport {
    TradeId    trade_id;        //  8
    OrderId    order_id;        //  8
    SequenceNo sequence;        //  8
    Timestamp  timestamp;       //  8
    Price      executed_price;  //  4
    Quantity   executed_qty;    //  4
    Quantity   leaves_qty;      //  4
    Quantity   cum_qty;         //  4
    ClientId   client_id;       //  4
    SymbolId   symbol_id;       //  2
    uint8_t    status;          //  1  (OrderStatus)
    uint8_t    side;            //  1  (OrderSide)
    uint8_t    reject_reason;   //  1  (RejectReason)
    uint8_t    _pad[7];         //  7 → total = 64

    ExecutionReport() noexcept { std::memset(this, 0, sizeof(ExecutionReport)); }

    static ExecutionReport fill(
        TradeId tid, OrderId oid, ClientId cid, SymbolId sym,
        uint8_t s, Price price, Quantity qty, Quantity leaves,
        Quantity cum, SequenceNo seq, Timestamp ts) noexcept
    {
        ExecutionReport r;
        r.trade_id      = tid;
        r.order_id      = oid;
        r.client_id     = cid;
        r.symbol_id     = sym;
        r.side          = s;
        r.executed_price = price;
        r.executed_qty  = qty;
        r.leaves_qty    = leaves;
        r.cum_qty       = cum;
        r.sequence      = seq;
        r.timestamp     = ts;
        r.status        = leaves == 0
            ? static_cast<uint8_t>(OrderStatus::FILLED)
            : static_cast<uint8_t>(OrderStatus::PARTIALLY_FILLED);
        return r;
    }

    static ExecutionReport reject(
        OrderId oid, ClientId cid, SymbolId sym, uint8_t s,
        RejectReason reason, SequenceNo seq, Timestamp ts) noexcept
    {
        ExecutionReport r;
        r.order_id      = oid;
        r.client_id     = cid;
        r.symbol_id     = sym;
        r.side          = s;
        r.reject_reason = static_cast<uint8_t>(reason);
        r.status        = static_cast<uint8_t>(OrderStatus::REJECTED);
        r.sequence      = seq;
        r.timestamp     = ts;
        return r;
    }

    static ExecutionReport cancel(
        OrderId oid, ClientId cid, SymbolId sym, uint8_t s,
        Quantity leaves, SequenceNo seq, Timestamp ts) noexcept
    {
        ExecutionReport r;
        r.order_id   = oid;
        r.client_id  = cid;
        r.symbol_id  = sym;
        r.side       = s;
        r.leaves_qty = leaves;
        r.status     = static_cast<uint8_t>(OrderStatus::CANCELLED);
        r.sequence   = seq;
        r.timestamp  = ts;
        return r;
    }
};
static_assert(sizeof(ExecutionReport) == 64, "ExecutionReport must be exactly 64 bytes");

// ============================================================================
// MarketData  (L1 snapshot, published to market data bus)
// ============================================================================
struct alignas(64) MarketData {
    Price      best_bid_price;   //  4
    Price      best_ask_price;   //  4
    Quantity   best_bid_qty;     //  4
    Quantity   best_ask_qty;     //  4
    Price      last_trade_price; //  4
    Quantity   last_trade_qty;   //  4
    Timestamp  timestamp;        //  8
    SequenceNo sequence;         //  8
    SymbolId   symbol_id;        //  2
    uint8_t    _pad[22];         // 22 → total = 64

    MarketData() noexcept { std::memset(this, 0, sizeof(MarketData)); }
    explicit MarketData(SymbolId sym) noexcept : MarketData() { symbol_id = sym; }
};
static_assert(sizeof(MarketData) == 64, "MarketData must be exactly 64 bytes");

// ============================================================================
// RDTSC latency stats helper (header-only, no heap)
// ============================================================================
struct LatencyStats {
    uint64_t min_cycles  = ~uint64_t{0};
    uint64_t max_cycles  = 0;
    uint64_t total_cycles = 0;
    uint64_t count       = 0;

    void record(uint64_t cycles) noexcept {
        if (cycles < min_cycles) min_cycles = cycles;
        if (cycles > max_cycles) max_cycles = cycles;
        total_cycles += cycles;
        ++count;
    }

    [[nodiscard]] uint64_t avg_cycles() const noexcept {
        return count > 0 ? total_cycles / count : 0;
    }

    [[nodiscard]] double avg_ns(double cpu_ghz) const noexcept {
        return static_cast<double>(avg_cycles()) / cpu_ghz;
    }

    [[nodiscard]] double min_ns(double cpu_ghz) const noexcept {
        return count > 0 ? static_cast<double>(min_cycles) / cpu_ghz : 0.0;
    }

    [[nodiscard]] double max_ns(double cpu_ghz) const noexcept {
        return static_cast<double>(max_cycles) / cpu_ghz;
    }

    void reset() noexcept {
        min_cycles = ~uint64_t{0};
        max_cycles = 0;
        total_cycles = 0;
        count = 0;
    }
};

} // namespace hft