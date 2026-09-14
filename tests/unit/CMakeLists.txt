add_executable(test_common test_common.cpp)
target_link_libraries(test_common PRIVATE mr::common GTest::gtest GTest::gtest_main)
add_test(NAME test_common COMMAND test_common)

add_executable(test_ring_buffer test_ring_buffer.cpp)
target_link_libraries(test_ring_buffer PRIVATE mr::common GTest::gtest GTest::gtest_main)
add_test(NAME test_ring_buffer COMMAND test_ring_buffer)

add_executable(test_clock test_clock.cpp)
target_link_libraries(test_clock PRIVATE mr::common GTest::gtest GTest::gtest_main)
add_test(NAME test_clock COMMAND test_clock)

add_executable(test_ten_second_ohlc test_ten_second_ohlc.cpp)
target_link_libraries(test_ten_second_ohlc PRIVATE mr::candle-engine mr::market-types GTest::gtest GTest::gtest_main)
add_test(NAME test_ten_second_ohlc COMMAND test_ten_second_ohlc)

add_executable(test_clock_architecture test_clock_architecture.cpp)
target_link_libraries(test_clock_architecture PRIVATE mr::market-core mr::decision-engine mr::risk-engine GTest::gtest GTest::gtest_main)
add_test(NAME test_clock_architecture COMMAND test_clock_architecture)

add_executable(test_live_multi_clock test_live_multi_clock.cpp)
target_link_libraries(test_live_multi_clock PRIVATE mr::market-core mr::candle-engine GTest::gtest GTest::gtest_main)
add_test(NAME test_live_multi_clock COMMAND test_live_multi_clock)

add_executable(test_stage2_runtime test_stage2_runtime.cpp)
target_link_libraries(test_stage2_runtime PRIVATE mr::market-core GTest::gtest GTest::gtest_main)
add_test(NAME test_stage2_runtime COMMAND test_stage2_runtime)
