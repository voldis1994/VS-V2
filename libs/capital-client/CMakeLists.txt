add_library(mr_capital_client STATIC
    src/capital_client.cpp
    src/capital_parse.cpp
    src/capital_instrument.cpp
    src/capital_session.cpp
    src/paper_broker.cpp)
add_library(mr::capital-client ALIAS mr_capital_client)
target_include_directories(mr_capital_client PUBLIC include)
target_link_libraries(mr_capital_client PUBLIC mr::common mr::market-types CURL::libcurl nlohmann_json::nlohmann_json)
target_compile_features(mr_capital_client PUBLIC cxx_std_20)

if(MR_BUILD_TESTS AND TARGET GTest::gtest_main)
    add_executable(mr_capital_client_tests tests/test_capital_parse.cpp)
    target_link_libraries(mr_capital_client_tests PRIVATE mr::capital-client GTest::gtest_main)
    add_test(NAME capital_client COMMAND mr_capital_client_tests)
endif()
