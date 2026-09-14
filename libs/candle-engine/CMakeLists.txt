add_library(mr_candle_engine STATIC
    src/candle_engine.cpp src/candle_builder.cpp src/second_builder.cpp src/ten_second_builder.cpp
    src/timeframe_aggregator.cpp src/candle_series.cpp src/candle_history.cpp
    src/candle_deduplicator.cpp src/candle_gap_handler.cpp src/candle_validator.cpp)
add_library(mr::candle-engine ALIAS mr_candle_engine)
target_include_directories(mr_candle_engine PUBLIC include)
target_link_libraries(mr_candle_engine PUBLIC mr::common mr::market-types)
target_compile_features(mr_candle_engine PUBLIC cxx_std_20)

if(MR_BUILD_TESTS AND TARGET GTest::gtest_main)
    add_executable(mr_candle_engine_tests
        tests/test_second.cpp tests/test_ten_second.cpp tests/test_boundary.cpp
        tests/test_gap.cpp tests/test_out_of_order.cpp tests/test_close_once.cpp)
    target_link_libraries(mr_candle_engine_tests PRIVATE mr::candle-engine GTest::gtest_main)
    add_test(NAME candle_engine COMMAND mr_candle_engine_tests)
endif()
