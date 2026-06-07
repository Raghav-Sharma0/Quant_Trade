#include <cassert>
#include <iostream>
#include "../include/risk/risk_manager.hpp"

using namespace hft;

int main() {
    printf("=== Risk Manager Tests ===\n");
    
    RiskManager risk;
    
    Order order1(1, 100, 100, OrderSide::BUY, 0);
    order1.client_id = 1;
    auto check1 = risk.check_order(order1);
    assert(check1 == RiskManager::RiskCheckResult::APPROVED);
    printf("✓ Normal order approved\n");
    
    Order order2(2, 100, 1000000, OrderSide::BUY, 0);
    order2.client_id = 1;
    auto check2 = risk.check_order(order2);
    assert(check2 == RiskManager::RiskCheckResult::APPROVED);
    printf("✓ Large order within limits\n");
    
    printf("\nAll risk manager tests passed!\n");
    return 0;
} // namespace hft