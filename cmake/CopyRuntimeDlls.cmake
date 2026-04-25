if(NOT DEFINED dst)
    message(FATAL_ERROR "dst is required")
endif()

if(NOT DEFINED dlls OR dlls STREQUAL "")
    return()
endif()

foreach(dll IN LISTS dlls)
    file(COPY "${dll}" DESTINATION "${dst}")
endforeach()
