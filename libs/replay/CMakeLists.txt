add_library(mr_replay STATIC
    src/replay_engine.cpp
    src/paper_order_gateway.cpp)
add_library(mr::replay ALIAS mr_replay)
target_include_directories(mr_replay PUBLIC include)
target_link_libraries(mr_replay PUBLIC
    mr::market-types
    mr::persistence
    mr::execution-engine
    mr::capital-client
    mr::common)
target_compile_features(mr_replay PUBLIC cxx_std_20)
