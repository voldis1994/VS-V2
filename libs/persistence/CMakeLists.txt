add_library(mr_persistence STATIC
    src/raw_event_storage.cpp src/repository.cpp src/transaction.cpp)
add_library(mr::persistence ALIAS mr_persistence)
target_include_directories(mr_persistence PUBLIC include)
target_link_libraries(mr_persistence PUBLIC mr::market-types mr::common)
target_compile_features(mr_persistence PUBLIC cxx_std_20)
