#pragma once

#include <cstdint>
#include "../common/types.hpp"
#include "../common/cacheline.hpp"
#include "market_data_bus.hpp"

namespace hft {

class TradePublisher {
public:
    explicit TradePublisher(MarketDataBus& bus) noexcept
        : bus_(bus)
        , published_count_(0)
    {}

    void publish(TradeId tid, OrderId bid, OrderId ask,
                 SymbolId sym, Price price, Quantity qty,
                 SequenceNo seq, Timestamp ts) noexcept
    {
        bus_.emit_trade(tid, bid, ask, sym, price, qty, seq, ts);
        ++published_count_;
        last_trade_price_[sym < MAX_SYMBOLS ? sym : 0] = price;
    }

    void publish_from_report(const ExecutionReport& rpt, Timestamp ts) noexcept {
        if (rpt.status != static_cast<uint8_t>(OrderStatus::FILLED) &&
            rpt.status != static_cast<uint8_t>(OrderStatus::PARTIALLY_FILLED)) {
            return;
        }
        publish(rpt.trade_id, rpt.order_id, rpt.order_id,
                rpt.symbol_id, rpt.executed_price, rpt.executed_qty,
                rpt.sequence, ts);
    }

    template <typename Callback>
    void replay(Callback&& cb) const noexcept {
        cb();
    }

    [[nodiscard]] uint64_t published_count() const noexcept { return published_count_; }

    [[nodiscard]] Price last_trade_price(SymbolId sym) const noexcept {
        return sym < MAX_SYMBOLS ? last_trade_price_[sym] : 0;
    }

private:
    MarketDataBus&               bus_;
    uint64_t                     published_count_ = 0;
    Price                        last_trade_price_[MAX_SYMBOLS] = {};
};

} // namespace hft
