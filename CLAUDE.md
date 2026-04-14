# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

The project uses CMake 3.25+ with Ninja, Clang++, and Conan 2 for dependency management. A Makefile wraps common operations:

```bash
make debug          # Build debug (default) — runs conan install + cmake configure + build
make release        # Build release
make asan           # Build with AddressSanitizer
make tsan           # Build with ThreadSanitizer
make ubsan          # Build with UBSanitizer
make test           # Run tests (debug preset)
make test PRESET=asan  # Run tests with a specific preset
make lint           # Run clang-tidy
make format         # Auto-format with clang-format
make format-check   # Check formatting without modifying
make conan-install  # Install Conan dependencies only
```

`make debug` also symlinks `compile_commands.json` to the project root for clangd.

To install dependencies without building: `make conan-install PRESET=debug`

## Dependencies (Conan 2)

Managed via `conanfile.py`:
- **imgui** (1.92.6-docking) — Dear ImGui with docking support
- **glfw** (3.4) — window/input backend
- **eigen** (3.4.0) — linear algebra
- **opengl/system** — system OpenGL

## Module Structure

```
src/
├── main.cpp              # Entry point
├── core/                 # Pump business logic (models, parameters)
│   ├── include/core/     # Public headers
│   └── src/
├── math/                 # Numerical methods, interpolation
│   ├── include/math/     # Public headers
│   └── src/
└── gui/                  # ImGui interface, rendering
    ├── include/gui/      # Public headers
    └── src/
```

Dependency graph: `gui → core → math → Eigen`, `gui → imgui, glfw, OpenGL`

## Adding Tests

Tests use CTest. Add test executables in `tests/CMakeLists.txt`:

```cmake
add_executable(test_foo test_foo.cpp)
target_link_libraries(test_foo PRIVATE core)  # link needed modules
add_test(NAME foo COMMAND test_foo)
```

Run a single test: `ctest --preset debug -R <test_name>`

## Code Style

Enforced by `.clang-format` (Mozilla-based) and `.clang-tidy`:

- **Formatting**: 2-space indent, 100-column limit, Mozilla brace style, pointers/refs left-aligned (`int* p`)
- **Naming** (enforced by clang-tidy `readability-identifier-naming`):
  - Classes/structs/enums: `CamelCase`
  - Functions/methods/members/params/variables: `camelBack`
  - Constants: `UPPER_CASE`
  - Private members: `camelBack_` (trailing underscore suffix)
  - Namespaces: `lower_case`
  - Enum constants: `CamelCase`
