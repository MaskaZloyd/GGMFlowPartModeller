cmake_minimum_required(VERSION 3.25)

project(GGMFlowPartModeller
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SANITIZER "" CACHE STRING "Sanitizer to enable: address | thread | undefined")
set_property(CACHE SANITIZER PROPERTY STRINGS "" address thread undefined)

option(ENABLE_COVERAGE "Enable LLVM source-based code coverage" OFF)
option(BUILD_TESTS "Build test suite" ON)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Wnon-virtual-dtor
    -fno-omit-frame-pointer
)

# Sanitizers
if(SANITIZER STREQUAL "address")
    add_compile_options(-fsanitize=address,leak)
    add_link_options(-fsanitize=address,leak)
elseif(SANITIZER STREQUAL "thread")
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
elseif(SANITIZER STREQUAL "undefined")
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=undefined)
elseif(NOT SANITIZER STREQUAL "")
    message(FATAL_ERROR "Unknown SANITIZER value: '${SANITIZER}'. "
        "Allowed: address | thread | undefined")
endif()

# Coverage
if(ENABLE_COVERAGE)
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()

# External dependencies (provided by Conan)
find_package(imgui REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(opengl_system REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(TBB REQUIRED)
find_package(spdlog REQUIRED)

add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
