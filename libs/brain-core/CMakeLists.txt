add_library(mr_brain_core STATIC src/brain_state.cpp src/brain_event_router.cpp src/market_brain.cpp)
add_library(mr::brain-core ALIAS mr_brain_core)
target_include_directories(mr_brain_core PUBLIC include)
target_link_libraries(mr_brain_core PUBLIC
    mr::common
    mr::market-types
    mr::candle-engine
    mr::feed-fusion
    mr::structure-engine
    mr::market-concepts)
target_compile_features(mr_brain_core PUBLIC cxx_std_20)
