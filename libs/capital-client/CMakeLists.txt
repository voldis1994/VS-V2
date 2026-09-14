add_library(mr_capital_client STATIC
    src/capital_client.cpp src/capital_instrument.cpp src/capital_session.cpp src/paper_broker.cpp)
add_library(mr::capital-client ALIAS mr_capital_client)
target_include_directories(mr_capital_client PUBLIC include)
target_link_libraries(mr_capital_client PUBLIC mr::common mr::market-types CURL::libcurl nlohmann_json::nlohmann_json)
target_compile_features(mr_capital_client PUBLIC cxx_std_20)
