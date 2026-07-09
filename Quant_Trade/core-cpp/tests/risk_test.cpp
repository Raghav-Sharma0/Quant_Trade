#include <cassert>
#include <iostream>

#include "../include/risk/risk_manager.hpp"

using namespace hft;

int main() {
  printf("=== Risk Manager Tests ===\n");

  RiskManager risk;

  Order order1(1, 100, 100, OrderSide::BUY, OrderType::LIMIT, 0, 1, 0);
  auto check1 = risk.check_order(order1);
  assert(check1 == RiskManager::RiskCheckResult::APPROVED);
  printf("✓ Normal order approved\n");

  Order order2(2, 100, 1000001, OrderSide::BUY, OrderType::LIMIT, 0, 1, 0);
  auto check2 = risk.check_order(order2);
  assert(check2 == RiskManager::RiskCheckResult::EXCEEDS_ORDER_QTY);
  printf("✓ Large order handled correctly\n");

  printf("\nAll risk manager tests passed!\n");
  return 0;
} // namespace hft