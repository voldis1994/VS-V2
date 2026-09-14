add_library(mr_pattern_engine STATIC
    src/pattern_encoder.cpp src/pattern_similarity.cpp src/pattern_cluster.cpp
    src/pattern_statistics.cpp src/pattern_engine.cpp)
add_library(mr::pattern-engine ALIAS mr_pattern_engine)
target_include_directories(mr_pattern_engine PUBLIC include)
target_link_libraries(mr_pattern_engine PUBLIC mr::perception-engine mr::structure-engine mr::memory-engine)
target_compile_features(mr_pattern_engine PUBLIC cxx_std_20)
