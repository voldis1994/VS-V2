add_library(mr_execution_engine STATIC
    src/order_builder.cpp src/capital_executor.cpp src/fill_tracker.cpp
    src/reconciliation.cpp src/execution_engine.cpp)
add_library(mr::execution-engine ALIAS mr_execution_engine)
target_include_directories(mr_execution_engine PUBLIC include)
target_link_libraries(mr_execution_engine PUBLIC mr::capital-client mr::risk-engine mr::decision-engine)
target_compile_features(mr_execution_engine PUBLIC cxx_std_20)
