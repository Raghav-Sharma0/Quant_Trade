#pragma once

#include <array>
#include <functional>
#include <cstdint>
#include "../common/types.hpp"
#include "../common/constants.hpp"
#include "../common/cacheline.hpp"
#include "../memory/object_pool.hpp"
#include "../order_book/order_book.hpp"
#include "../order_book/order_node.hpp"
#include "execution_report.hpp"

namespace hft {

constexpr size_t MAX_FILLS_PER_ORDER = 64;

struct MatchResult {
    ExecutionReport reports[MAX_FILLS_PER_ORDER + 1];
    uint32_t        count     = 0;
    uint32_t        fill_count = 0;
    bool            matched   = false;
};

constexpr size_t ENGINE_NODE_POOL_SIZE = 1 << 20;

class MatchingEngine {
public:
    explicit MatchingEngine(size_t num_symbols = MAX_SYMBOLS)
        : num_symbols_(num_symbols)
        , trade_counter_(1)
    {
        for (size_t i = 0; i < num_symbols && i < MAX_SYMBOLS; ++i) {
            books_[i] = new (&book_storage_[i]) OrderBook(static_cast<SymbolId>(i));
        }
    }

    ~MatchingEngine() noexcept {
        for (size_t i = 0; i < num_symbols_; ++i) {
            if (books_[i]) books_[i]->~OrderBook();
        }
    }

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    void submit_order(const Order& order, MatchResult& result, Timestamp now) noexcept {
        result.count = result.fill_count = 0;
        result.matched = false;

        if (HFT_UNLIKELY(order.symbol_id >= num_symbols_)) {
            result.reports[0] = ExecutionReport::reject(
                order.order_id, order.client_id, order.symbol_id,
                order.side, RejectReason::UNKNOWN_SYMBOL, order.sequence, now);
            result.count = 1;
            return;
        }

        OrderNode* taker = node_pool_.construct();
        if (HFT_UNLIKELY(!taker)) {
            result.reports[0] = ExecutionReport::reject(
                order.order_id, order.client_id, order.symbol_id,
                order.side, RejectReason::NONE, order.sequence, now);
            result.count = 1;
            return;
        }
        taker->from_order(order);
        taker->timestamp = now;

        OrderBook& book = *books_[order.symbol_id];

        struct FillRecord { OrderNode* maker; Quantity qty; Price price; };
        FillRecord fills[MAX_FILLS_PER_ORDER];
        uint32_t fill_cnt = 0;

        FillCallback cb = [&](OrderNode* t, OrderNode* maker,
                               Quantity qty, Price price) {
            if (fill_cnt < MAX_FILLS_PER_ORDER) {
                fills[fill_cnt++] = {maker, qty, price};
            }
        };

        book.match_order(taker, cb);

        if (order.is_fok() && taker->remain_qty > 0) {
            for (uint32_t i = 0; i < fill_cnt; ++i) {
                fills[i].maker->remain_qty += fills[i].qty;
                fills[i].maker->status = static_cast<uint8_t>(OrderStatus::ACCEPTED);
                book.add_order(fills[i].maker);
            }
            node_pool_.destroy(taker);

            result.reports[0] = ExecutionReport::reject(
                order.order_id, order.client_id, order.symbol_id,
                order.side, RejectReason::NONE, order.sequence, now);
            result.count = 1;
            return;
        }

        Quantity cum_qty = 0;
        for (uint32_t i = 0; i < fill_cnt; ++i) {
            cum_qty += fills[i].qty;
            const Quantity leaves = taker->orig_qty - cum_qty;

            result.reports[result.count++] = ExecutionReport::fill(
                trade_counter_,
                order.order_id, order.client_id, order.symbol_id,
                order.side, fills[i].price, fills[i].qty, leaves, cum_qty,
                order.sequence, now);

            if (result.count < MAX_FILLS_PER_ORDER + 1) {
                const Quantity maker_leaves = fills[i].maker->remain_qty;
                result.reports[result.count++] = ExecutionReport::fill(
                    trade_counter_,
                    fills[i].maker->order_id, fills[i].maker->client_id,
                    fills[i].maker->symbol_id, fills[i].maker->side,
                    fills[i].price, fills[i].qty, maker_leaves,
                    fills[i].maker->orig_qty - maker_leaves,
                    fills[i].maker->sequence, now);

                if (fills[i].maker->remain_qty == 0) {
                    node_pool_.destroy(fills[i].maker);
                }
            }

            ++trade_counter_;
            result.fill_count = fill_cnt;
            result.matched    = true;
        }

        if (taker->remain_qty > 0 && !order.is_ioc() && !order.is_fok()) {
            taker->status = static_cast<uint8_t>(OrderStatus::ACCEPTED);
            book.add_order(taker);
        } else if (taker->remain_qty > 0) {
            node_pool_.destroy(taker);
        } else {
            node_pool_.destroy(taker);
        }
    }

    ExecutionReport cancel_order(SymbolId sym, OrderId oid,
                                 ClientId cid, SequenceNo seq, Timestamp now) noexcept {
        if (HFT_UNLIKELY(sym >= num_symbols_)) {
            return ExecutionReport::reject(oid, cid, sym, 0,
                                           RejectReason::UNKNOWN_SYMBOL, seq, now);
        }

        OrderNode* node = books_[sym]->cancel_order(oid);
        if (HFT_UNLIKELY(!node)) {
            return ExecutionReport::reject(oid, cid, sym, 0,
                                           RejectReason::NONE, seq, now);
        }

        ExecutionReport rpt = ExecutionReport::cancel(
            oid, cid, sym, node->side, node->remain_qty, seq, now);
        node_pool_.destroy(node);
        return rpt;
    }

    void modify_order(SymbolId sym, OrderId oid, ClientId cid,
                      Price new_price, Quantity new_qty,
                      MatchResult& result, SequenceNo seq, Timestamp now) noexcept {
        if (HFT_UNLIKELY(sym >= num_symbols_)) {
            result.reports[0] = ExecutionReport::reject(
                oid, cid, sym, 0, RejectReason::UNKNOWN_SYMBOL, seq, now);
            result.count = 1;
            return;
        }

        bool ok = books_[sym]->modify_order(oid, new_price, new_qty);
        if (!ok) {
            result.reports[0] = ExecutionReport::reject(
                oid, cid, sym, 0, RejectReason::NONE, seq, now);
            result.count = 1;
            return;
        }
        result.count = 0;
    }

    [[nodiscard]] MarketData get_market_data(SymbolId sym) const noexcept {
        if (HFT_UNLIKELY(sym >= num_symbols_)) return MarketData(sym);
        return books_[sym]->snapshot();
    }

    [[nodiscard]] OrderBook& book(SymbolId sym) noexcept {
        return *books_[sym];
    }

    [[nodiscard]] const OrderBook& book(SymbolId sym) const noexcept {
        return *books_[sym];
    }

    [[nodiscard]] size_t node_pool_used() const noexcept {
        return node_pool_.used_count();
    }

private:
    size_t   num_symbols_;
    uint64_t trade_counter_;

    ObjectPool<OrderNode, ENGINE_NODE_POOL_SIZE> node_pool_;

    alignas(CACHELINE_SIZE) std::array<std::byte, sizeof(OrderBook)> book_storage_[MAX_SYMBOLS];
    OrderBook* books_[MAX_SYMBOLS] = {};
};

} // namespace hft
