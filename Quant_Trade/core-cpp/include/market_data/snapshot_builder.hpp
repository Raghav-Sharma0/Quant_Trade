#pragma once

#include <array>
#include <cstdint>
#include "../common/types.hpp"
#include "../common/constants.hpp"
#include "../common/cacheline.hpp"
#include "../order_book/order_book.hpp"
#include "market_data_bus.hpp"

namespace hft {

struct BookLevel {
    Price    price    = 0;
    Quantity quantity = 0;
    uint32_t count    = 0;
};

struct L10Snapshot {
    SymbolId   symbol_id  = 0;
    SequenceNo sequence   = 0;
    Timestamp  timestamp  = 0;
    uint8_t    bid_levels = 0;
    uint8_t    ask_levels = 0;
    BookLevel  bids[10];
    BookLevel  asks[10];
};

class SnapshotBuilder {
public:
    explicit SnapshotBuilder(MarketDataBus& bus) noexcept
        : bus_(bus)
        , snapshot_seq_(0)
    {}

    void on_book_update(const OrderBook& book, Timestamp ts) noexcept {
        const MarketData md = book.snapshot();

        bus_.emit_snapshot(
            book.symbol(),
            md.best_bid_price, md.best_bid_qty,
            md.best_ask_price, md.best_ask_qty,
            ++snapshot_seq_, ts);
    }

    [[nodiscard]] static MarketData build_l1(
        const OrderBook& book, Timestamp ts, SequenceNo seq) noexcept
    {
        MarketData md = book.snapshot();
        md.timestamp = ts;
        md.sequence  = seq;
        return md;
    }

    static void build_l10(const OrderBook& book, L10Snapshot& out,
                           Timestamp ts, SequenceNo seq) noexcept {
        out.symbol_id  = book.symbol();
        out.sequence   = seq;
        out.timestamp  = ts;
        out.bid_levels = 0;
        out.ask_levels = 0;

        book.for_each_bid_level([&](Price p, Quantity q, uint32_t cnt) {
            if (out.bid_levels < 10) {
                out.bids[out.bid_levels++] = {p, q, cnt};
            }
        }, 10);

        book.for_each_ask_level([&](Price p, Quantity q, uint32_t cnt) {
            if (out.ask_levels < 10) {
                out.asks[out.ask_levels++] = {p, q, cnt};
            }
        }, 10);
    }

    [[nodiscard]] SequenceNo last_snapshot_seq() const noexcept { return snapshot_seq_; }

private:
    MarketDataBus& bus_;
    SequenceNo     snapshot_seq_;
};

} // namespace hft
