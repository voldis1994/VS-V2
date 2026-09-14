add_executable(test_replay test_replay.cpp)
target_link_libraries(test_replay PRIVATE mr::replay mr::persistence GTest::gtest GTest::gtest_main)
add_test(NAME test_replay COMMAND test_replay)

add_executable(test_deterministic_replay test_deterministic_replay.cpp)
target_link_libraries(test_deterministic_replay PRIVATE
    mr::market-core mr::replay mr::persistence mr::market-types GTest::gtest GTest::gtest_main)
add_test(NAME test_deterministic_replay COMMAND test_deterministic_replay)
