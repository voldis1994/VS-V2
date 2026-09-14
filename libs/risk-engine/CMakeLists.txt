add_library(mr_risk_engine STATIC
  src/risk_engine.cpp
  src/risk_weight_config.cpp)
add_library(mr::risk-engine ALIAS mr_risk_engine)
target_include_directories(mr_risk_engine PUBLIC include)
target_link_libraries(mr_risk_engine PUBLIC mr::position-brain mr::decision-engine)
target_compile_features(mr_risk_engine PUBLIC cxx_std_20)
