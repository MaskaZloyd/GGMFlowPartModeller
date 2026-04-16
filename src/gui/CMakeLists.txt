add_library(gui STATIC
    src/application.cpp
    src/theme.cpp
    src/window/app_window.cpp
    src/renderer/fbo.cpp
    src/renderer/geometry_renderer.cpp
    src/commands/edit_command.cpp
    src/commands/undo_stack.cpp
    src/layout/dockspace.cpp
    src/solver_status_display.cpp
    src/panels/params_panel.cpp
    src/panels/geometry_panel.cpp
    src/panels/charts_panel.cpp
    src/panels/settings_panel.cpp
    src/panels/log_panel.cpp
    src/backends/imgui_impl_glfw.cpp
    src/backends/imgui_impl_opengl3.cpp
    src/backends/implot/implot.cpp
    src/backends/implot/implot_items.cpp
)

target_include_directories(gui
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/backends
            ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/implot
)

target_link_libraries(gui
    PUBLIC  core
    PRIVATE imgui::imgui glfw opengl::opengl
)
