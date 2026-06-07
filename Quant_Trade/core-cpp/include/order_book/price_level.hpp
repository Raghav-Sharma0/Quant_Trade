#pragma once

#include <cstddef>
#include <cstdint>
#include "../common/types.hpp"
#include "../common/cacheline.hpp"
#include "order_node.hpp"

namespace hft {

// ============================================================================
// PriceLevel
//
// FIFO queue of OrderNodes at a single price point.
//
// Design:
//  - Intrusive doubly-linked list (no std::list, no allocations here)
//  - O(1) push_back (tail insert — FIFO, time priority)
//  - O(1) pop_front (head remove — matches oldest first)
//  - O(1) remove(node*) given a direct pointer (cancel by pointer from lookup)
//  - Tracks aggregate_qty and order_count for fast best-level queries
//
// Fits in ~48 bytes — multiple levels fit per cache line when packed in an array.
// ============================================================================
struct PriceLevel {
    OrderNode* head       = nullptr;  //  8
    OrderNode* tail       = nullptr;  //  8
    Quantity   agg_qty    = 0;        //  4  total remaining quantity at this level
    uint32_t   order_cnt  = 0;        //  4  number of live orders
    Price      price      = 0;        //  4
    uint8_t    _pad[12]   = {};       // 12 → total = 40 bytes

    // -------------------------------------------------------------------------
    // push_back — append node at tail (time priority: newest = last)
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // pop_front — remove and return the oldest (head) node
    // Returns nullptr if empty.
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // remove — O(1) removal given direct pointer (from order lookup table)
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // Peek front without removing
    // -------------------------------------------------------------------------
    [[nodiscard]] OrderNode* front() const noexcept { return head; }

    // -------------------------------------------------------------------------
    // Adjust quantity after partial fill (called by matching engine)
    // -------------------------------------------------------------------------
    void reduce_qty(Quantity qty) noexcept {
        agg_qty -= qty;
    }

    [[nodiscard]] bool     empty() const noexcept { return head == nullptr; }
    [[nodiscard]] uint32_t count() const noexcept { return order_cnt; }
    [[nodiscard]] Quantity total() const noexcept { return agg_qty; }
};

} // namespace hft
