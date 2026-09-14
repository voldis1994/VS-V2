add_library(mr_market_core STATIC
    src/pipeline.cpp
    src/runtime.cpp
    src/brain_runtime.cpp
    src/brain_json.cpp
    src/event_dispatcher.cpp
    src/subscription_manager.cpp
    src/intent_dispatcher.cpp
    src/health_monitor.cpp
    src/live_multi_clock_path.cpp
    src/capital_env.cpp
    src/episode_replay.cpp
    src/weight_bundle.cpp
    src/candidate_replay_eval.cpp
    src/live_capital_bootstrap.cpp
    src/model_bundle_store.cpp
)
add_library(mr::market-core ALIAS mr_market_core)
target_include_directories(mr_market_core PUBLIC include src)
target_link_libraries(mr_market_core PUBLIC
    mr::brain-core
    mr::normalization
    mr::data-quality
    mr::feed-fusion
    mr::candle-engine
    mr::perception-engine
    mr::structure-engine
    mr::market-concepts
    mr::microstructure-engine
    mr::scenario-engine
    mr::prediction-engine
    mr::decision-engine
    mr::risk-engine
    mr::execution-engine
    mr::position-brain
    mr::capital-client
    mr::telemetry
    mr::memory-engine
    mr::replay
    mr::common
    nlohmann_json::nlohmann_json
    CURL::libcurl
)
target_compile_features(mr_market_core PUBLIC cxx_std_20)

add_executable(market-core src/main.cpp)
target_link_libraries(market-core PRIVATE mr::market-core)
target_compile_features(market-core PRIVATE cxx_std_20)

add_executable(candidate-replay-eval src/candidate_replay_eval_main.cpp)
target_link_libraries(candidate-replay-eval PRIVATE mr::market-core)
target_compile_features(candidate-replay-eval PRIVATE cxx_std_20)
