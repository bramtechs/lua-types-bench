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
- Visual Studio 2022 (MSVC)
- CMake ≥ 3.20
- vcpkg at `C:\dev\vcpkg` (or update `CMAKE_TOOLCHAIN_FILE` below)

### Steps

```bash
# Configure — vcpkg installs sol2 3.5.0, Lua 5.4.8, Google Benchmark 1.9.5
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Copy DLLs next to the exe (first time only)
cp build/vcpkg_installed/x64-windows/bin/*.dll build/Release/

# Run
./build/Release/luatypetest.exe --benchmark_format=console

# Capture JSON
./build/Release/luatypetest.exe --benchmark_format=json > results.json
```

> **Note:** vcpkg.json pins Lua to 5.4.8 because sol2 3.5.0 does not support Lua 5.5.

---

## Benchmark Results

Machine: Intel Core i7-12700H (20 threads @ 2803 MHz), Windows 11, MSVC 19.50, Release build.

```
Benchmark                   Time             CPU   Iterations  items_per_second
BM_Usertypes/100       359076 ns       322323 ns         2036   310.248k/s
BM_Usertypes/1000     3551500 ns      3447770 ns          213   290.043k/s
BM_Usertypes/10000   59123189 ns     53453947 ns           19   187.077k/s
BM_Tables/100          255030 ns       228795 ns         2800   437.073k/s
BM_Tables/1000        2750684 ns      2287946 ns          280   437.073k/s
BM_Tables/10000      27194764 ns     20625000 ns           25   484.848k/s
```

`items_per_second` = inner-loop iterations per second (one full set of 4-type operations per item).

### Per-item cost and slowdown ratio

| n | Usertypes (ns/item) | Tables (ns/item) | Ratio |
|---|---------------------|------------------|-------|
| 100 | ~3 220 | ~2 288 | **1.41×** |
| 1 000 | ~3 448 | ~2 288 | **1.51×** |
| 10 000 | ~5 345 | ~2 063 | **2.59×** |

---

## Analysis

**At small n (100 iterations), usertypes are ~1.4× slower than tables.**
Each field access on a usertype goes through Lua's metamethod dispatch (`__index`), whereas a plain table field access is a direct hash-table lookup. Constructing a usertype also allocates a Lua *userdata* object (a heap-allocated block with a metatable) instead of a lightweight table.

**The gap widens significantly at n=10 000 (2.6× slower).**
This is primarily **garbage-collector pressure**. Each loop iteration allocates several userdata objects (one per struct). At 10 000 iterations the GC cycles more frequently to collect the dead userdata, which is more expensive than collecting dead tables because userdata have `__gc` finalizers registered by sol2. Plain tables are GC'd in bulk with no per-object finalization cost.

**Plain tables are the right choice for pure Lua-side math objects.**
If the struct only needs to be seen from Lua, a plain table is faster and simpler. Usertypes pay off when:
- You need a C++-side struct to be shared by reference between C++ and Lua without copying
- You want type safety / metamethod enforcement on the Lua side
- The struct is long-lived (not created/destroyed in a tight loop)

For hot-path, iteration-heavy geometry code in Lua, prefer tables or preallocate and reuse usertype objects rather than constructing them per-iteration.
