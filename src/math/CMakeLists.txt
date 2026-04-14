add_library(math STATIC
    src/bezier.cpp
    src/nurbs.cpp
    src/arc_length.cpp
)

target_include_directories(math PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(math PUBLIC Eigen3::Eigen)
