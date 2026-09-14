add_library(mr_position_brain STATIC
  src/position_brain.cpp
  src/position_weight_config.cpp)
add_library(mr::position-brain ALIAS mr_position_brain)
target_include_directories(mr_position_brain PUBLIC include)
target_link_libraries(mr_position_brain PUBLIC
  mr::decision-engine
  mr::prediction-engine
  mr::perception-engine
  mr::common)
target_compile_features(mr_position_brain PUBLIC cxx_std_20)
