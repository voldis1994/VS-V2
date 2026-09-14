add_library(mr_prediction_engine STATIC
  src/prediction_engine.cpp
  src/prediction_weight_config.cpp)
add_library(mr::prediction-engine ALIAS mr_prediction_engine)
target_include_directories(mr_prediction_engine PUBLIC include)
target_link_libraries(mr_prediction_engine PUBLIC
  mr::scenario-engine
  mr::pattern-engine
  mr::perception-engine
  mr::structure-engine
  mr::microstructure-engine
  mr::market-types
  mr::common)
target_compile_features(mr_prediction_engine PUBLIC cxx_std_20)
