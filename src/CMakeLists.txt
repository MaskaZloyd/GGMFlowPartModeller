add_subdirectory(math)
add_subdirectory(core)
add_subdirectory(gui)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE gui)
