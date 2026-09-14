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

add_executable(test_structure_brain test_structure_brain.cpp)
target_link_libraries(test_structure_brain PRIVATE mr::market-core mr::structure-engine mr::market-concepts GTest::gtest GTest::gtest_main)
add_test(NAME test_structure_brain COMMAND test_structure_brain)

add_executable(test_microstructure_brain test_microstructure_brain.cpp)
target_link_libraries(test_microstructure_brain PRIVATE mr::market-core mr::microstructure-engine mr::structure-engine mr::brain-core GTest::gtest GTest::gtest_main)
add_test(NAME test_microstructure_brain COMMAND test_microstructure_brain)

add_executable(test_prediction_decision_brain test_prediction_decision_brain.cpp)
target_link_libraries(test_prediction_decision_brain PRIVATE
  mr::prediction-engine
  mr::decision-engine
  mr::structure-engine
  mr::microstructure-engine
  mr::brain-core
  GTest::gtest
  GTest::gtest_main)
add_test(NAME test_prediction_decision_brain COMMAND test_prediction_decision_brain)


add_executable(test_risk_execution_position_brain test_risk_execution_position_brain.cpp)
target_link_libraries(test_risk_execution_position_brain PRIVATE
  mr::risk-engine
  mr::execution-engine
  mr::position-brain
  mr::brain-core
  mr::decision-engine
  mr::prediction-engine
  GTest::gtest
  GTest::gtest_main)
add_test(NAME test_risk_execution_position_brain COMMAND test_risk_execution_position_brain)

add_executable(test_stage6_lifecycle_integration test_stage6_lifecycle_integration.cpp)
target_link_libraries(test_stage6_lifecycle_integration PRIVATE
  mr::market-core
  mr::execution-engine
  mr::position-brain
  mr::risk-engine
  mr::brain-core
  GTest::gtest
  GTest::gtest_main)
add_test(NAME test_stage6_lifecycle_integration COMMAND test_stage6_lifecycle_integration)

add_executable(test_stage7_memory_replay test_stage7_memory_replay.cpp)
target_link_libraries(test_stage7_memory_replay PRIVATE
  mr::market-core
  mr::memory-engine
  mr::replay
  mr::execution-engine
  mr::position-brain
  mr::prediction-engine
  mr::decision-engine
  mr::risk-engine
  mr::brain-core
  GTest::gtest
  GTest::gtest_main)
add_test(NAME test_stage7_memory_replay COMMAND test_stage7_memory_replay)

add_executable(test_stage8_candidate_replay test_stage8_candidate_replay.cpp)
target_link_libraries(test_stage8_candidate_replay PRIVATE
  mr::market-core
  mr::memory-engine
  mr::replay
  mr::execution-engine
  mr::position-brain
  mr::prediction-engine
  mr::decision-engine
  mr::risk-engine
  mr::brain-core
  GTest::gtest
  GTest::gtest_main)
add_test(NAME test_stage8_candidate_replay COMMAND test_stage8_candidate_replay)
