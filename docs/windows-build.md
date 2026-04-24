# Windows Build

Prerequisites:

- Visual Studio 2022 with "Desktop development with C++"
- CMake 3.25+
- Ninja
- Python 3 with Conan 2

Run commands from "Developer PowerShell for VS 2022" in the repository root:

```powershell
conan profile detect --force
conan install . `
  --output-folder=build/release `
  -s build_type=Release `
  -s compiler.cppstd=23 `
  -c tools.cmake.cmaketoolchain:user_presets="" `
  --build=missing
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
.\build\release\src\GGMFlowPartModeller.exe
```

To create a folder that can be copied to another machine:

```powershell
cmake --install build/release --prefix dist/GGMFlowPartModeller
```

To create a ZIP package:

```powershell
cmake --build --preset release --target package
```

The install/package output includes the executable, the current `imgui.ini`
from the repository root, and runtime DLLs detected by CMake.

For Debug, replace `release` with `debug` and use `-s build_type=Debug`.

The preset uses Ninja and the compiler available in the current shell. Use a
Visual Studio Developer shell for MSVC, or set `CXX` before configure if you
want a different compiler.
