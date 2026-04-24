# GGM Flow Part Modeller

C++23-приложение для построения меридианного сечения насоса,
FEM-расчета потока и обратного проектирования геометрии по целевой кривой
площади.

## Возможности

- Параметрическая геометрия втулки и покрывного диска.
- Расчет потока на FEM-сетке.
- Оптимизация геометрии по нормированной кривой площади.
- GUI на Dear ImGui / ImPlot / GLFW / OpenGL.

## Зависимости

- CMake 3.25+
- Conan 2
- Ninja
- C++23 compiler: GCC, Clang или MSVC 2022

Зависимости GUI и core подтягиваются через Conan.

## Сборка Linux

```bash
make release
make test PRESET=release
./build/release/src/GGMFlowPartModeller
```

## Сборка Windows

Запускать из `Developer PowerShell for VS 2022`:

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

Подробнее: [docs/windows-build.md](docs/windows-build.md).

## Распространение

```bash
make package PRESET=release
```

ZIP появится в `build/release`. В пакет попадает исполняемый файл и текущий
`imgui.ini` из корня проекта.

Для папки без архива:

```bash
make install PRESET=release INSTALL_DIR=dist/GGMFlowPartModeller
```

## Структура

- `src/math` - геометрические и численные примитивы.
- `src/core` - геометрия, сетка, FEM, оптимизация.
- `src/gui` - интерфейс и OpenGL-рендеринг.
- `tests` - Catch2-тесты.
