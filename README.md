# Lua Usertype vs Table Benchmark (sol2)

Measures the real-world cost of [sol2](https://github.com/ThePhD/sol2) usertypes versus plain Lua tables when doing typical math/geometry work inside Lua loops.

---

## What Is Being Benchmarked

Two equivalent Lua `do_work(n)` functions are benchmarked head-to-head:

| Version | Data representation |
|---------|---------------------|
| **Usertypes** | C++ structs exposed via `sol2::new_usertype` — field access goes through sol2's `__index`/`__newindex` metamethods |
| **Tables** | Plain `{x=..., y=...}` Lua tables — field access is native Lua hash-table lookup |

Both functions perform the **same operations** per iteration so the only variable is the type representation.

---

## The Four Types

| Type | Fields | Operations in the loop |
|------|--------|------------------------|
| `Vector2` | `float x, y` | Dot product with another Vector2 |
| `Vector3` | `float x, y, z` | Dot product with another Vector3 |
| `RectF` | `float x, y, w, h` | Area (`w * h`) and point-in-rect test |
| `Point` | `int x, y` | Squared distance from origin (`x²+y²`) |

Per loop iteration:
1. Construct two `Vector2` values, compute dot product → accumulate
2. Construct two `Vector3` values, compute dot product → accumulate
3. Construct a `RectF`, compute area → accumulate
4. Construct a `Point`, compute `x²+y²` → accumulate
5. Point-in-rect test using `Vector2` and `RectF` → conditionally accumulate

The accumulated `sum` is returned so Lua cannot dead-code-eliminate any of the work.

---

## Build Instructions

### Prerequisites
- Visual Studio 2022 with the ClangCL workload (for the `clang-cl` preset)
- CMake ≥ 3.20
- vcpkg at `C:\dev\vcpkg` (or update `CMAKE_TOOLCHAIN_FILE` in `CMakePresets.json`)

### Steps

```bash
# Configure — vcpkg installs sol2 3.5.0, Lua 5.4.8, Google Benchmark 1.9.5
cmake --preset msvc        # or: cmake --preset clang-cl

# Build
cmake --build build-msvc --config Release
# or
cmake --build build-clangcl --config Release

# Copy DLLs next to the exe (first time only)
cp build-msvc/vcpkg_installed/x64-windows/bin/*.dll build-msvc/Release/

# Run
./build-msvc/Release/luatypetest.exe --benchmark_format=console
```

> **Note:** vcpkg.json pins Lua to 5.4.8 because sol2 3.5.0 does not support Lua 5.5.

---

## Benchmark Results

Machine: Intel Core i7-12700H (20 threads @ 2803 MHz), Windows 11, Release build.
`items_per_second` = inner-loop iterations per second (one full set of 4-type operations).

### MSVC 19.50

```
Benchmark                   Time             CPU   Iterations  items_per_second
BM_Usertypes/100       372515 ns       368369 ns         2036   271.467k/s
BM_Usertypes/1000     3726992 ns      3667840 ns          213   272.640k/s
BM_Usertypes/10000   74642880 ns     70312500 ns           20   142.222k/s
BM_Tables/100          256545 ns       249023 ns         3200   401.569k/s
BM_Tables/1000        2653773 ns      2259036 ns          249   442.667k/s
BM_Tables/10000      26709729 ns     25390625 ns           24   393.846k/s
```

| n | Usertypes (ns/item) | Tables (ns/item) | Ratio |
|---|---------------------|------------------|-------|
| 100 | ~3 684 | ~2 490 | **1.48×** |
| 1 000 | ~3 668 | ~2 259 | **1.62×** |
| 10 000 | ~7 031 | ~2 539 | **2.77×** |

### clang-cl (VS ClangCL toolset)

```
Benchmark                   Time             CPU   Iterations  items_per_second
BM_Usertypes/100       353150 ns       337672 ns         2036   296.145k/s
BM_Usertypes/1000     3599216 ns      3447770 ns          213   290.043k/s
BM_Usertypes/10000   34865657 ns     34970238 ns           21   285.957k/s
BM_Tables/100          109129 ns       106720 ns         7467   937.035k/s
BM_Tables/1000        1087688 ns      1087684 ns          747   919.385k/s
BM_Tables/10000      10971995 ns     10986328 ns           64   910.222k/s
```

| n | Usertypes (ns/item) | Tables (ns/item) | Ratio |
|---|---------------------|------------------|-------|
| 100 | ~3 377 | ~1 067 | **3.16×** |
| 1 000 | ~3 448 | ~1 088 | **3.17×** |
| 10 000 | ~3 497 | ~1 099 | **3.18×** |

---

## Analysis

### Tables: clang-cl is ~2.3× faster than MSVC

clang-cl produces significantly tighter code for the plain-table path (~1 080 ns/item vs ~2 430 ns/item with MSVC). The Lua table hash-lookup loop benefits strongly from clang's optimizer. The usertype path shows only a modest improvement (~3 430 ns vs ~4 790 ns), because that path is dominated by Lua C API calls and metamethod dispatch — overhead that cannot be optimized away by the compiler.

### MSVC: GC pressure grows with n, clang-cl does not

With MSVC the usertype ratio climbs from **1.48× → 2.77×** as n increases, because MSVC's generated code is slower overall, so the Lua GC fires more frequently relative to useful work. With clang-cl the ratio is flat at **~3.17×** across all n — clang runs fast enough that GC cycles are a smaller fraction of total time.

### Choosing between usertypes and tables

Plain tables are the right choice for pure Lua-side math objects constructed in tight loops. Usertypes pay off when:
- The object is long-lived (not created/destroyed per iteration)
- You need a C++-side struct shared by reference between C++ and Lua without copying
- You want type safety or metamethod enforcement from Lua

For hot-path geometry code, prefer tables or preallocate and reuse usertype objects rather than constructing them per-iteration.
