add_library(mr_common STATIC
    src/config.cpp
)
add_library(mr::common ALIAS mr_common)
target_include_directories(mr_common PUBLIC include)
target_compile_features(mr_common PUBLIC cxx_std_20)
target_link_libraries(mr_common PUBLIC fmt::fmt yaml-cpp::yaml-cpp)
