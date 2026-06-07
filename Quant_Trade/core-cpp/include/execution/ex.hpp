#pragma once

#include "../common/types.hpp"
#include <cstring>

namespace hft {

class ExecutionEngine {
public:
    struct SerializedOrder {
        uint8_t data[32];
        size_t size;
        SerializedOrder() : size(20) {}
    };
    
    SerializedOrder serialize(const Order& order) const noexcept {
        SerializedOrder so;
        uint8_t* p = so.data;
        
        *p++ = 0x01;
        
        uint64_t oid = order.order_id;
        for (int i = 7; i >= 0; --i) {
            *p++ = (oid >> (i * 8)) & 0xFF;
        }
        
        *p++ = (order.symbol_id >> 8) & 0xFF;
        *p++ = order.symbol_id & 0xFF;
        
        *p++ = order.side;
        
        uint32_t price = order.price;
        *p++ = (price >> 24) & 0xFF;
        *p++ = (price >> 16) & 0xFF;
        *p++ = (price >> 8) & 0xFF;
        *p++ = price & 0xFF;
        
        uint32_t qty = order.quantity;
        *p++ = (qty >> 24) & 0xFF;
        *p++ = (qty >> 16) & 0xFF;
        *p++ = (qty >> 8) & 0xFF;
        *p++ = qty & 0xFF;
        
        return so;
    }
};

} // namespace hft