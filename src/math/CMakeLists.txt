add_library(math STATIC
    src/placeholder.cpp
)

target_include_directories(math PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(math PUBLIC Eigen3::Eigen)
