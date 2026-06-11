#pragma once

#include <cstddef>
#include <cstdint>
#include "../common/types.hpp"
#include "../common/cacheline.hpp"
#include "order_node.hpp"

namespace hft {

struct PriceLevel {
    OrderNode* head       = nullptr;
    OrderNode* tail       = nullptr;
    Quantity   agg_qty    = 0;
    uint32_t   order_cnt  = 0;
    Price      price      = 0;
    uint8_t    _pad[12]   = {};

    void push_back(OrderNode* node) noexcept {
        node->prev = tail;
        node->next = nullptr;
        if (tail) {
            tail->next = node;
        } else {
            head = node;
        }
        tail = node;
        agg_qty  += node->remain_qty;
        ++order_cnt;
    }

    [[nodiscard]] OrderNode* pop_front() noexcept {
        if (HFT_UNLIKELY(!head)) return nullptr;
        OrderNode* node = head;
        head = node->next;
        if (head) {
            head->prev = nullptr;
        } else {
            tail = nullptr;
        }
        agg_qty  -= node->remain_qty;
        --order_cnt;
        node->next = node->prev = nullptr;
        return node;
    }

    void remove(OrderNode* node) noexcept {
        if (node->prev) {
            node->prev->next = node->next;
        } else {
            head = node->next;
        }
        if (node->next) {
            node->next->prev = node->prev;
        } else {
            tail = node->prev;
        }
        agg_qty  -= node->remain_qty;
        --order_cnt;
        node->next = node->prev = nullptr;
    }

    [[nodiscard]] OrderNode* front() const noexcept { return head; }

    void reduce_qty(Quantity qty) noexcept {
        agg_qty -= qty;
    }

    [[nodiscard]] bool     empty() const noexcept { return head == nullptr; }
    [[nodiscard]] uint32_t count() const noexcept { return order_cnt; }
    [[nodiscard]] Quantity total() const noexcept { return agg_qty; }
};

} // namespace hft
