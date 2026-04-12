add_library(gui STATIC
    src/placeholder.cpp
)

target_include_directories(gui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(gui
    PUBLIC core
    PRIVATE imgui::imgui glfw opengl::opengl
)
