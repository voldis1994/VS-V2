add_library(mr_execution_engine STATIC
    src/order_builder.cpp
    src/capital_executor.cpp
    src/fill_tracker.cpp
    src/reconciliation.cpp
    src/order_gateway.cpp
    src/execution_weight_config.cpp
    src/execution_engine.cpp)
add_library(mr::execution-engine ALIAS mr_execution_engine)
target_include_directories(mr_execution_engine PUBLIC include)
target_link_libraries(mr_execution_engine PUBLIC mr::capital-client mr::decision-engine mr::common)
target_compile_features(mr_execution_engine PUBLIC cxx_std_20)
