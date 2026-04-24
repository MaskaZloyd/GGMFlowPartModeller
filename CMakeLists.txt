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

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
        -fno-omit-frame-pointer
    )
endif()

# Sanitizers
if(NOT SANITIZER STREQUAL "" AND MSVC)
    message(FATAL_ERROR "SANITIZER presets are supported only with GCC/Clang-style toolchains")
elseif(SANITIZER STREQUAL "address")
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
    if(MSVC)
        message(FATAL_ERROR "ENABLE_COVERAGE is supported only with LLVM/GCC-style toolchains")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()

# External dependencies (provided by Conan)
find_package(imgui REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(opengl_system REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(TBB REQUIRED)
find_package(spdlog REQUIRED)

add_subdirectory(src)

if(BUILD_TESTS)
    find_package(Catch2 REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()

install(FILES
    "${CMAKE_SOURCE_DIR}/README.md"
    DESTINATION .
)

install(FILES
    "${CMAKE_SOURCE_DIR}/imgui.ini"
    DESTINATION .
    OPTIONAL
)

if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
endif()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "GGM")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Pump flow part modelling and inverse geometry design"
)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_FILE_NAME
    "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)

include(CPack)
