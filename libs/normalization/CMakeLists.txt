add_library(mr_normalization STATIC
    src/normalizer.cpp src/quote_normalizer.cpp
    src/candle_normalizer.cpp src/timestamp_normalizer.cpp)
add_library(mr::normalization ALIAS mr_normalization)
target_include_directories(mr_normalization PUBLIC include)
target_link_libraries(mr_normalization PUBLIC mr::common mr::market-types)
target_compile_features(mr_normalization PUBLIC cxx_std_20)
