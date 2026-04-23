# Test suite using Catch2.
# Add one executable per logical group. Link against the module under test.
# Tests are auto-registered with CTest via catch_discover_tests().
#
# Usage:
# make test                   # run all tests (debug preset)
# make test PRESET=asan       # run under AddressSanitizer
# ctest --preset debug -R nurbs   # run a single named test

include(Catch)

# ---------------------------------------------------------------------------
# math — pure numerical primitives, no GL/GLFW dependency
# ---------------------------------------------------------------------------
add_executable(test_math
    test_math_bezier.cpp
    test_math_nurbs.cpp
    test_math_arc_length.cpp
)
target_link_libraries(test_math PRIVATE Catch2::Catch2WithMain math)
catch_discover_tests(test_math)

# ---------------------------------------------------------------------------
# core — pump geometry, grid, FEM solver, serialization
# ---------------------------------------------------------------------------
add_executable(test_core
    test_core_geometry.cpp
    test_core_strip_grid.cpp
    test_core_fem_solver.cpp
    test_core_serialization.cpp
    test_core_velocity_field.cpp
)
target_link_libraries(test_core PRIVATE Catch2::Catch2WithMain core)
catch_discover_tests(test_core)

# ---------------------------------------------------------------------------
# gui (logic only) — undo stack; no rendering required
# ---------------------------------------------------------------------------
add_executable(test_gui_commands
    test_gui_undo_stack.cpp
)
target_link_libraries(test_gui_commands PRIVATE Catch2::Catch2WithMain gui)
catch_discover_tests(test_gui_commands)

# ---------------------------------------------------------------------------
# bench_hotpaths — standalone benchmark binary for `perf record`.
# NOT registered with CTest. Invoked manually from the release preset.
# ---------------------------------------------------------------------------
add_executable(bench_hotpaths bench_hotpaths.cpp)
target_link_libraries(bench_hotpaths PRIVATE core math)
