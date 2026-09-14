add_executable(market-core src/main.cpp src/pipeline.cpp)
target_include_directories(market-core PRIVATE include)
target_link_libraries(market-core PRIVATE
    mr::brain-core mr::normalization mr::data-quality mr::feed-fusion
    mr::perception-engine mr::structure-engine mr::market-concepts
    mr::microstructure-engine mr::scenario-engine mr::prediction-engine
    mr::decision-engine mr::telemetry mr::common)
target_compile_features(market-core PRIVATE cxx_std_20)
