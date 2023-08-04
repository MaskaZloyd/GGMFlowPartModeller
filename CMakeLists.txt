cmake_minimum_required(VERSION 3.23)

project(GGMFlowPartModeller VERSION 0.1 LANGUAGES CXX)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>"
    )
endif()

# -------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CXX_EXTENSIONS YES)

message(STATUS "Architecture: " ${ARCHITECTURE})

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# -------------------------------------------------------------------
add_subdirectory(lib/qcustomplot)

# -------------------------------------------------------------------
set(MAIN_SRCS
    src/ggmfpm_entrypoint.cpp
    src/ggmfpm_mainwindow.hpp
    src/ggmfpm_mainwindow.cpp
)

# -------------------------------------------------------------------
find_package(QT NAMES Qt6 EQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)
find_package(
    Qt${QT_VERSION_MAJOR}
    REQUIRED
    COMPONENTS Core Widgets OpenGL OpenGLWidgets
)

qt_wrap_ui(MAIN_SRCS ui/ggmfpm_mainwindow.ui)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME} MANUAL_FINALIZATION ${MAIN_SRCS} ${MAIN_UI_SRC})
endif()

# -------------------------------------------------------------------
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE Qt6::Core Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets QCustomPlot
)

target_include_directories(${PROJECT_NAME} SYSTEM BEFORE PUBLIC lib/qcustomplot)

set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

# -------------------------------------------------------------------
install(
    TARGETS ${PROJECT_NAME}
    BUNDLE
    DESTINATION .
    LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

# -------------------------------------------------------------------
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
)
