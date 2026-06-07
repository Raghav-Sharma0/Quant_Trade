#pragma once

#include "../common/types.hpp"
#include <map>
#include <list>

namespace hft {

class OrderBookSide {
private:
    struct PriceLevel {
        uint32_t total_qty;
        std::list<Order> orders;
        PriceLevel() : total_qty(0) {}
    };
    std::map<uint32_t, PriceLevel> levels;
    
public:
    OrderBookSide() = default;
    ~OrderBookSide() = default;
    
    void add_order(const Order& order) noexcept {
        levels[order.price].total_qty += order.quantity;
        levels[order.price].orders.push_back(order);
    }

    bool remove_order(uint32_t price, uint64_t order_id) noexcept {
        auto it = levels.find(price);
        if (it == levels.end()) return false;
        auto& orders = it->second.orders;
        for (auto oit = orders.begin(); oit != orders.end(); ++oit) {
            if (oit->order_id == order_id) {
                it->second.total_qty -= oit->quantity;
                orders.erase(oit);
                if (it->second.orders.empty()) {
                    levels.erase(it);
                }
                return true;
            }
        }
        return false;
    }

    bool remove_order_by_id(uint64_t order_id) noexcept {
        for (auto& [price, level] : levels) {
            auto& orders = level.orders;
            for (auto oit = orders.begin(); oit != orders.end(); ++oit) {
                if (oit->order_id == order_id) {
                    level.total_qty -= oit->quantity;
                    orders.erase(oit);
                    if (level.orders.empty()) {
                        levels.erase(price);
                    }
                    return true;
                }
            }
        }
        return false;
    }
    
    uint32_t best_price(bool is_buy) const noexcept {
        if (levels.empty()) return 0;
        if (is_buy) return levels.rbegin()->first;
        return levels.begin()->first;
    }

    uint32_t best_qty(bool is_buy) const noexcept {
        auto price = best_price(is_buy);
        if (price == 0) return 0;
        auto it = levels.find(price);
        return it != levels.end() ? it->second.total_qty : 0;
    }
  
    const std::list<Order>* orders_at_price(uint32_t price) const noexcept {
        auto it = levels.find(price);
        if (it == levels.end()) return nullptr;
        return &it->second.orders;
    }
    
    const std::list<Order>* best_orders(bool is_buy) const noexcept {
        auto price = best_price(is_buy);
        return orders_at_price(price);
    }
    
    size_t depth() const noexcept {
        return levels.size();
    }
    
    void clear() noexcept {
        levels.clear();
    }
};

class OrderBook {
private:
    OrderBookSide buy_side;
    OrderBookSide sell_side;
    uint16_t symbol_id;
    
public:
    OrderBook(uint16_t sym = 0) : symbol_id(sym) {}
    ~OrderBook() = default;
    
    uint16_t get_symbol() const noexcept { return symbol_id; }
    
    void add_order(const Order& order) noexcept {
        if (order.side == static_cast<uint8_t>(OrderSide::BUY)) {
            buy_side.add_order(order);
        } else {
            sell_side.add_order(order);
        }
    }

    bool remove_order(const Order& order) noexcept {
        if (order.side == static_cast<uint8_t>(OrderSide::BUY)) {
            return buy_side.remove_order(order.price, order.order_id);
        } else {
            return sell_side.remove_order(order.price, order.order_id);
        }
    }

    bool remove_order_by_id(uint64_t order_id) noexcept {
        if (buy_side.remove_order_by_id(order_id)) return true;
        return sell_side.remove_order_by_id(order_id);
    }

    MarketData get_snapshot() const noexcept {
        MarketData md(symbol_id);
        md.best_bid_price = buy_side.best_price(true);
        md.best_ask_price = sell_side.best_price(false);
        md.best_bid_qty = buy_side.best_qty(true);
        md.best_ask_qty = sell_side.best_qty(false);
        return md;
    }

    const std::list<Order>* get_counterparty_orders(bool incoming_is_buy) const noexcept {
        if (incoming_is_buy) {
            return sell_side.best_orders(false);
        } else {
            return buy_side.best_orders(true);
        }
    }
    
    size_t buy_depth() const noexcept { return buy_side.depth(); }
    size_t sell_depth() const noexcept { return sell_side.depth(); }
    
    void clear() noexcept {
        buy_side.clear();
        sell_side.clear();
    }
};

}  // namespace hft