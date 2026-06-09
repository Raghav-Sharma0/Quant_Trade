#pragma once

#include <cstdint>
#include <cstring>
#include <array>

namespace hft {

using OrderId    = uint64_t;
using TradeId    = uint64_t;
using Price      = uint32_t;
using Quantity   = uint32_t;
using SymbolId   = uint16_t;
using ClientId   = uint32_t;
using Timestamp  = uint64_t;
using SequenceNo = uint64_t;

enum class OrderSide : uint8_t {
    BUY  = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    LIMIT  = 0,
    MARKET = 1,
    IOC    = 2,
    FOK    = 3 
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

struct alignas(64) Order {
    OrderId   order_id;
    Price     price;
    Quantity  quantity;
    Quantity  filled_qty;
    ClientId  client_id;
    Timestamp timestamp;
    SequenceNo sequence;
    SymbolId  symbol_id;
    uint8_t   side;
    uint8_t   type;
    uint8_t   status;
    uint8_t   reject_reason;
    uint8_t   _pad[14];

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

struct alignas(64) Trade {
    TradeId   trade_id;
    OrderId   bid_order_id;
    OrderId   ask_order_id;
    Price     price;
    Quantity  quantity;
    Timestamp timestamp;
    SequenceNo sequence;
    SymbolId  symbol_id;
    uint8_t   _pad[14];

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

struct alignas(64) ExecutionReport {
    TradeId    trade_id;
    OrderId    order_id;
    SequenceNo sequence;
    Timestamp  timestamp;
    Price      executed_price;
    Quantity   executed_qty;
    Quantity   leaves_qty;
    Quantity   cum_qty;
    ClientId   client_id;
    SymbolId   symbol_id;
    uint8_t    status;
    uint8_t    side;
    uint8_t    reject_reason;
    uint8_t    _pad[7];

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

struct alignas(64) MarketData {
    Price      best_bid_price;
    Price      best_ask_price;
    Quantity   best_bid_qty;
    Quantity   best_ask_qty;
    Price      last_trade_price;
    Quantity   last_trade_qty;
    Timestamp  timestamp;
    SequenceNo sequence;
    SymbolId   symbol_id;
    uint8_t    _pad[22];

    MarketData() noexcept { std::memset(this, 0, sizeof(MarketData)); }
    explicit MarketData(SymbolId sym) noexcept : MarketData() { symbol_id = sym; }
};
static_assert(sizeof(MarketData) == 64, "MarketData must be exactly 64 bytes");

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
