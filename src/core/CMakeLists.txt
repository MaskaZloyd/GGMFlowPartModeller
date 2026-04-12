add_library(core STATIC
    src/placeholder.cpp
)

target_include_directories(core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(core PUBLIC math)
