add_library(core STATIC
    src/geometry.cpp
    src/pump_model.cpp
    src/serialization.cpp
    src/strip_grid.cpp
    src/fem_solver.cpp
    src/streamline_extractor.cpp
    src/velocity_field.cpp
    src/area_profile.cpp
    src/flow_solver.cpp
    src/async_flow_solver.cpp
    src/target_area_curve.cpp
    src/geometry_optimizer.cpp
    src/async_geometry_optimizer.cpp
    src/logging.cpp
)

target_include_directories(core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(core
    PUBLIC math spdlog::spdlog
    PRIVATE nlohmann_json::nlohmann_json TBB::tbb
)
