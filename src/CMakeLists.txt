add_subdirectory(math)
add_subdirectory(core)
add_subdirectory(gui)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE gui)
ggm_copy_runtime_dlls(${PROJECT_NAME})

if(WIN32)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME_DEPENDENCY_SET ggm_runtime_dependencies
        RUNTIME DESTINATION .
    )

    install(RUNTIME_DEPENDENCY_SET ggm_runtime_dependencies
        PRE_EXCLUDE_REGEXES
            "api-ms-"
            "ext-ms-"
        POST_EXCLUDE_REGEXES
            ".*[Ww]indows[\\\\/].*"
            ".*[Ss]ystem32[\\\\/].*"
        RUNTIME DESTINATION .
        LIBRARY DESTINATION .
    )
else()
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION .
    )
endif()
