add_executable(test_multi_feed test_multi_feed.cpp)
target_link_libraries(test_multi_feed PRIVATE
    mr::feed-fusion mr::data-quality mr::normalization mr::market-types mr::common
    GTest::gtest GTest::gtest_main)
add_test(NAME test_multi_feed COMMAND test_multi_feed)

add_executable(test_pipeline_integration test_pipeline_integration.cpp)
target_link_libraries(test_pipeline_integration PRIVATE
    mr::brain-core mr::normalization mr::data-quality mr::feed-fusion
    mr::candle-engine mr::market-types mr::common
    GTest::gtest GTest::gtest_main)
add_test(NAME test_pipeline_integration COMMAND test_pipeline_integration)
