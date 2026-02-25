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
1. Construct two `Vector2` values; compute `+`, `-`, `*scalar`, `/scalar`; accumulate fields from each result
2. Construct two `Vector3` values; compute `+`, `-`, `*scalar`, `/scalar`; accumulate fields from each result
3. Construct a `RectF`, compute area (`w * h`) → accumulate
4. Construct a `Point`, compute `x²+y²` → accumulate
5. Point-in-rect test using `Vector2` and `RectF` → conditionally accumulate

The accumulated `sum` is returned so Lua cannot dead-code-eliminate any of the work.

In the **usertype** script, vector arithmetic uses Lua operator syntax (`v2a + v2b`, `v2a * 2.0`, etc.) which dispatches through sol2's registered `__add`, `__sub`, `__mul`, `__div` metamethods. In the **table** script the same arithmetic is done component-wise by hand (`{x=v2a.x+v2b.x, y=v2a.y+v2b.y}`, etc.), so both scripts perform an identical number of floating-point operations.

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
BM_Usertypes/100       565643 ns       578125 ns         1000   172.973k/s
BM_Usertypes/1000     5807545 ns      5625000 ns          100   177.778k/s
BM_Usertypes/10000   55114745 ns     52556818 ns           11   190.270k/s
BM_Tables/100          250827 ns       245536 ns         2800   407.273k/s
BM_Tables/1000        2534964 ns      2508361 ns          299   398.667k/s
BM_Tables/10000      25024764 ns     24553571 ns           28   407.273k/s
```

| n | Usertypes (ns/item) | Tables (ns/item) | Ratio |
|---|---------------------|------------------|-------|
| 100 | ~5 781 | ~2 455 | **2.35×** |
| 1 000 | ~5 625 | ~2 508 | **2.24×** |
| 10 000 | ~5 256 | ~2 455 | **2.14×** |

### clang-cl (VS ClangCL toolset)

```
Benchmark                   Time             CPU   Iterations  items_per_second
BM_Usertypes/100       567157 ns       546875 ns         1000   182.857k/s
BM_Usertypes/1000     5608119 ns      5468750 ns          100   182.857k/s
BM_Usertypes/10000   56407560 ns     51562500 ns           10   193.939k/s
BM_Tables/100          257108 ns       256319 ns         2987   390.139k/s
BM_Tables/1000        2539381 ns      2511161 ns          280   398.222k/s
BM_Tables/10000      25496442 ns     24639423 ns           26   405.854k/s
```

| n | Usertypes (ns/item) | Tables (ns/item) | Ratio |
|---|---------------------|------------------|-------|
| 100 | ~5 469 | ~2 563 | **2.13×** |
| 1 000 | ~5 469 | ~2 511 | **2.18×** |
| 10 000 | ~5 156 | ~2 464 | **2.09×** |

---

## Analysis

### Both compilers agree: usertypes cost ~2–2.4× vs tables

Every vector operator (`+`, `-`, `*`, `/`) dispatches through a sol2 metamethod, crossing the C++/Lua boundary twice (once to look up the metamethod, once to call it) and allocating a fresh userdata object for the result. Plain tables perform the same arithmetic inline and allocate ordinary Lua tables. The overhead is consistent and predictable across both compilers.

### GC pressure is now symmetric

Unlike the earlier single-operator benchmark, each iteration now allocates the same number of intermediate objects in both the usertype and table scripts (4 result objects per vector type). This is why the ratio stays flat across all values of n for both compilers — neither side accumulates disproportionate GC load.

### Compiler choice barely affects the usertype path

Usertypes: MSVC ~5 554 ns/item vs clang-cl ~5 365 ns/item — only a ~3% difference. The bottleneck is Lua C API overhead and metamethod dispatch, not the code the compiler generates for the lambda bodies.

Tables: MSVC ~2 473 ns/item vs clang-cl ~2 513 ns/item — effectively identical here too (unlike a previous lighter workload where clang-cl had a larger edge, the added table allocations bring both compilers to the same level).

### Choosing between usertypes and tables

Plain tables are the right choice for pure Lua-side math objects constructed in tight loops. Usertypes pay off when:
- The object is long-lived (not created/destroyed per iteration)
- You need a C++-side struct shared by reference between C++ and Lua without copying
- You want type safety or metamethod enforcement from Lua

For hot-path geometry code, prefer tables or preallocate and reuse usertype objects rather than constructing them per-iteration.
