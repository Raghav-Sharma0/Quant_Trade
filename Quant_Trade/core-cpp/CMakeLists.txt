cmake_minimum_required(VERSION 3.15)
project(hft_system CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -march=native -fno-exceptions -fno-rtti")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(hft_core INTERFACE)
target_include_directories(hft_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(test_order_book tests/order_book_test.cpp)
target_link_libraries(test_order_book hft_core)

add_executable(test_matching_engine tests/matching_engine_test.cpp)
target_link_libraries(test_matching_engine hft_core)

add_executable(test_risk tests/risk_test.cpp)
target_link_libraries(test_risk hft_core)

add_executable(test_strategy tests/strategy_test.cpp)
target_link_libraries(test_strategy hft_core)

add_executable(bench_latency benchmarks/latency_bench.cpp)
target_link_libraries(bench_latency hft_core)

add_executable(bench_order_book benchmarks/order_book_bench.cpp)
target_link_libraries(bench_order_book hft_core)

add_executable(bench_matching_engine benchmarks/matching_engine_bench.cpp)
target_link_libraries(bench_matching_engine hft_core)

enable_testing()
add_test(NAME OrderBookTest COMMAND test_order_book)
add_test(NAME MatchingEngineTest COMMAND test_matching_engine)
add_test(NAME RiskTest COMMAND test_risk)
add_test(NAME StrategyTest COMMAND test_strategy)