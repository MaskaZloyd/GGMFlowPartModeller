# Refactoring Design: C++ Core Guidelines + Performance

**Date:** 2026-04-16  
**Branch:** `refactoring/cpp-core-guidelines`  
**Approach:** Targeted high-priority (Plan Б)

---

## Scope

Apply C++ Core Guidelines to GGMFlowPartModeller, fix per-frame allocations in the renderer, replace naive NURBS evaluation with the proper de Boor algorithm, and profile before/after with `perf record` + `perf report`.

Out of scope: file-dialog integration, new features, unrelated modules.

---

## Section 1 — Performance

### 1.1 NURBS de Boor algorithm (`math/nurbs.cpp`)

**Problem:** `basisFunc` is a recursive Cox-de Boor function. For each of 1500 sample points it iterates over all N control points, each call recursing to depth `degree`. Result: O(N · 4^degree) per curve evaluation. Called twice on every slider drag.

**Fix:** Replace with the standard de Boor algorithm:
1. Binary search for the active knot span → O(log n) per point.
2. Compute only `degree+1 = 3` non-zero basis values using a flat triangular recurrence → O(degree²) per point.

`basisFunc` is removed entirely. `evaluate` is rewritten. `buildFromSegments` is unchanged.

**Validation:** Existing tests `evaluate: single arc segment — all points on circle` and `evaluate: straight segment — collinear points` must still pass with tolerance 1e-10. Add a new test comparing old vs. new output on a two-arc curve.

### 1.2 GeometryRenderer scratch buffer (`gui/src/renderer/geometry_renderer.cpp`)

**Problem:** `DrawSession::drawPolyline` allocates `std::vector<float>` on every call, every frame. With hub + shroud curves (1500 pts each) plus up to 10 streamlines, this is ~12 allocations per frame at 60 fps.

**Fix:** Add `std::vector<float> scratchVertices_` to `GeometryRenderer`. Pass it by reference into `DrawSession` as `scratchVertices_`. `drawPolyline` and `buildGridVertices` call `clear()` and `reserve()` on it — allocation only on first use, reused every frame.

`DrawSession` stores a `std::vector<float>&` reference. `GeometryRenderer::render` passes `scratchVertices_` when constructing `DrawSession`.

### 1.3 strip_grid Laplace ping-pong (`core/src/strip_grid.cpp`)

**Problem:** `smoothInterior` copies the full node vector (`prev = nodes`) at the start of each of 20 Jacobi iterations. For nh=200, m=80 this is 16 000 `Vec2` copies × 20 = 320 000 copies per grid build.

**Fix:** Allocate two buffers before the loop (`cur`, `nxt`), alternate via index swap. No allocation inside the loop.

```cpp
auto bufs = std::array<std::vector<math::Vec2>, 2>{nodes, nodes};
int src = 0;
for (int iter = 0; iter < iterations; ++iter) {
    int dst = 1 - src;
    // update bufs[dst] from bufs[src]
    src = dst;
}
nodes = std::move(bufs[src]);
```

**Validation:** `buildStripGrid: boundary node counts equal nh` and `hub nodes have r=0` tests must still pass.

### 1.4 Remove FEM diagnostic dump (`core/src/fem_solver.cpp`)

**Problem:** Debug `dumpRow` block (lines 197–216) runs on every FEM solve. Uses `std::ostringstream` in production code inside the worker thread.

**Fix:** Delete the block unconditionally. The information is available via the test suite and debugger when needed.

---

## Section 2 — C++ Core Guidelines Compliance

### 2.1 Magic number π (`core/src/flow_solver.cpp:195`)

Replace `3.14159265358979323846` with `std::numbers::pi`. (ES.45)

### 2.2 Dead code removal (`gui/src/application.cpp`, `gui/include/gui/application.hpp`)

`Application::processEdit` is declared, implemented, and never called. Remove from both files.

### 2.3 Fbo move constructor (`gui/src/renderer/fbo.cpp`)

`samples_` is not reset in the moved-from object. Apply `std::exchange(other.samples_, 0)` consistently with the existing pattern for GL handles. (C.21)

### 2.4 Uniform error vocabulary for Fbo (`core/include/core/error.hpp`, `gui/src/renderer/fbo.cpp`)

`Fbo::resize` returns `std::expected<void, std::string>`. Add `CoreError::RenderFailed` to the enum and `toString` switch. Change `Fbo::resize` to return `Result<void>`. Update the one call site in `geometry_panel.cpp`. (I.4)

### 2.5 PumpModel::rebuildGeometry — propagate error (`core/src/pump_model.cpp`, `core/include/core/pump_model.hpp`)

`rebuildGeometry()` silently discards the `CoreError` from `buildGeometry`. Change return type to `Result<void>`. Update all call sites (`application.cpp`: `create`, `run`, `handleUndo`, `handleRedo`, `handleOpen`, `handleNew`) to log the specific error via `core::toString(result.error())`. (E.6)

### 2.6 Deduplicate SolverStatus display (`gui/src/layout/dockspace.cpp`, `gui/src/panels/settings_panel.cpp`)

Both files contain an identical `switch (status)` → `{color, label}`. Extract to:

```cpp
// gui/include/gui/layout/dockspace.hpp  (or a new solver_status.hpp)
struct StatusDisplay { ImVec4 color; const char* label; };
[[nodiscard]] StatusDisplay solverStatusDisplay(core::SolverStatus status) noexcept;
```

Both call sites replaced with one call. (C.4, DRY)

### 2.7 Eliminate global PendingEdit (`gui/src/panels/params_panel.cpp`, `gui/include/gui/panels/params_panel.hpp`, `gui/include/gui/application.hpp`, `gui/src/application.cpp`)

`static PendingEdit pendingEdit` is a hidden mutable global. Move it to an explicit `ParamsPanelState` struct:

```cpp
struct ParamsPanelState { std::optional<EditCommand> pending; };
```

`drawParamsPanel` signature becomes:
```cpp
ParamsPanelResult drawParamsPanel(const PumpParams& params, ParamsPanelState& state);
```

`Application` holds `ParamsPanelState panelState_` as a member field. (I.2, CP.3)

---

## Section 3 — Profiling

### Tool

`perf record -g -F 999` + `perf report --stdio`. No GUI required, no code instrumentation.

### Benchmark binary

Add `tests/bench_hotpaths.cpp` and a CMake target `bench_hotpaths` (not registered with CTest). The benchmark exercises:
- `math::evaluate` — 1000 iterations × 1500-point NURBS eval
- `buildStripGrid` + `solveFem` — 50 iterations of the full solver pipeline
- Flat-point conversion loop (equivalent to `drawPolyline` inner work)

### Process

1. Build release: `make release`
2. Record before: `perf record -g -F 999 -o perf_before.data -- build/release/tests/bench_hotpaths`
3. Report: `perf report --stdio --no-children -n -i perf_before.data 2>&1 | head -60`
4. Apply Section 1 fixes
5. Rebuild release, record after: `perf record -g -F 999 -o perf_after.data -- ...`
6. Compare top-10 symbols before vs. after
7. Document speedup in commit message

### Expected findings

| Symbol | Expected rank before | Expected rank after |
|--------|---------------------|---------------------|
| `basisFunc` | top-3 | absent |
| `solveFem` / TBB internals | top-3 | top-1 (FEM dominates) |
| `smoothInterior copy` | visible | lower |

---

## Commit sequence

Each item is a separate commit following the existing style (`feat:`, `fix:`, `refactor:`, `perf:`):

1. `perf: replace Cox-de Boor recursion with de Boor knot-span algorithm`
2. `perf: reuse scratch vertex buffer in GeometryRenderer`
3. `perf: eliminate per-iteration copy in Laplace grid smoother`
4. `refactor: remove FEM diagnostic dump`
5. `fix: replace pi literal with std::numbers::pi`
6. `refactor: remove dead Application::processEdit method`
7. `fix: reset samples_ in Fbo move constructor`
8. `refactor: unify error vocabulary — Fbo::resize returns Result<void>`
9. `refactor: propagate CoreError from PumpModel::rebuildGeometry`
10. `refactor: extract solverStatusDisplay to eliminate duplicated switch`
11. `refactor: make ParamsPanelState explicit, remove global PendingEdit`
12. `perf: add bench_hotpaths and record before/after perf profiles`
