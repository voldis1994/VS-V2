add_library(mr_data_quality STATIC
    src/quality_engine.cpp src/freshness.cpp src/latency.cpp src/anomaly.cpp)
add_library(mr::data-quality ALIAS mr_data_quality)
target_include_directories(mr_data_quality PUBLIC include)
target_link_libraries(mr_data_quality PUBLIC mr::common mr::market-types)
target_compile_features(mr_data_quality PUBLIC cxx_std_20)
