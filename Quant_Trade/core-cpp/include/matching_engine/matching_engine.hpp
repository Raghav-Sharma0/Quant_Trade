#pragma once

#include "../common/types.hpp"
#include "../order_book/order_book.hpp"
#include <vector>
#include <unordered_map>

namespace hft {

class MatchingEngine {
private:
    std::vector<OrderBook> books;
    uint64_t trade_counter;
    std::unordered_map<uint64_t, std::tuple<uint16_t, uint32_t, uint8_t>> order_lookup;
    
public:
    explicit MatchingEngine(size_t num_symbols) 
        : books(num_symbols, OrderBook(0)), trade_counter(0) {
        for (size_t i = 0; i < num_symbols; ++i) {
            books[i] = OrderBook(i);
        }
    }
    
    ~MatchingEngine() = default;

    struct MatchResult {
        std::vector<Trade> trades;
        uint32_t remaining_qty;
    };
    
    MatchResult match_order(Order incoming) noexcept {
        MatchResult result;
        result.remaining_qty = incoming.quantity;
        
        if (incoming.symbol_id >= books.size()) {
            return result;
        }
        
        OrderBook& book = books[incoming.symbol_id];
   
        while (result.remaining_qty > 0) {
            const auto* counterparty_orders = 
                book.get_counterparty_orders(
                    incoming.side == static_cast<uint8_t>(OrderSide::BUY));
            
            if (!counterparty_orders || counterparty_orders->empty()) {
                break;
            }
            
            const Order& existing = counterparty_orders->front();
            
            if (!should_match(incoming, existing)) {
                break;
            }
            
            uint32_t match_qty = std::min(result.remaining_qty, existing.remaining());
            
            Trade trade(++trade_counter, 
                       incoming.side == static_cast<uint8_t>(OrderSide::BUY) ? 
                           incoming.order_id : existing.order_id,
                       incoming.side == static_cast<uint8_t>(OrderSide::BUY) ? 
                           existing.order_id : incoming.order_id,
                       existing.price, match_qty, incoming.symbol_id);
            
            result.trades.push_back(trade);
            result.remaining_qty -= match_qty;
            
            if (existing.remaining() == match_qty) {
                order_lookup.erase(existing.order_id);
                book.remove_order(existing);
            }
        }
        
        if (result.remaining_qty > 0) {
            incoming.quantity = result.remaining_qty;
            book.add_order(incoming);
            order_lookup[incoming.order_id] = 
                std::make_tuple(incoming.symbol_id, incoming.price, incoming.side);
        }
        
        return result;
    }

    bool cancel_order(uint16_t symbol_id, uint64_t order_id) noexcept {
        auto it = order_lookup.find(order_id);
        if (it == order_lookup.end()) {
            return false;
        }
        
        auto [stored_symbol, price, side] = it->second;
        
        if (stored_symbol != symbol_id) {
            return false;
        }
        
        OrderBook& book = books[symbol_id];
        bool removed = book.remove_order_by_id(order_id);
        
        if (!removed) {
            return false;
        }
        
        order_lookup.erase(it);
        return true;
    }
    
    MarketData get_market_data(uint16_t symbol_id) const noexcept {
        if (symbol_id >= books.size()) {
            return MarketData(symbol_id);
        }
        return books[symbol_id].get_snapshot();
    }
    
private:
    bool should_match(const Order& incoming, const Order& existing) const noexcept {
        if (incoming.side == static_cast<uint8_t>(OrderSide::BUY)) {
            return incoming.price >= existing.price;
        } else {
            return incoming.price <= existing.price;
        }
    }
};

} // namespace hft