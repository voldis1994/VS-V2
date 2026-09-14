add_library(mr_memory_engine STATIC
    src/memory_engine.cpp
    src/episode_recorder.cpp
    src/episode_store.cpp)
add_library(mr::memory-engine ALIAS mr_memory_engine)
target_include_directories(mr_memory_engine PUBLIC include)
target_link_libraries(mr_memory_engine PUBLIC
    mr::perception-engine
    mr::brain-core
    mr::prediction-engine
    mr::decision-engine
    mr::risk-engine
    mr::execution-engine
    mr::position-brain
    mr::market-types
    mr::market-concepts
    mr::structure-engine
    mr::microstructure-engine
    mr::common
    nlohmann_json::nlohmann_json)
target_compile_features(mr_memory_engine PUBLIC cxx_std_20)
