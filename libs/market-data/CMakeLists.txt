add_library(mr_market_data STATIC
    src/market_data_service.cpp src/quote_stream.cpp src/subscription.cpp src/ohlc_source.cpp)
add_library(mr::market-data ALIAS mr_market_data)
target_include_directories(mr_market_data PUBLIC include)
target_link_libraries(mr_market_data PUBLIC mr::common mr::market-types)
target_compile_features(mr_market_data PUBLIC cxx_std_20)
