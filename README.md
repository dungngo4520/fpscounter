# fpscounter

A professional, header-only C++17 FPS counter library for real-time applications.

## Quick Start

```cpp
#include <fpscounter/fpscounter.hpp>
#include <iostream>

int main() {
    fpscounter::FPSCounter<> counter;  // default: steady_clock

    // In your game/render loop:
    // Note: the first Update() call returns 0.0 (measurement skipped)
    double fps = counter.Update();
    if (fps > 0.0) {
        std::cout << fps << " FPS\n";
    }

    return 0;
}
```

Copy `include/fpscounter/fpscounter.hpp` into your project, include it, and call `Update()` once per frame. The first call returns `0.0` — the first real measurement comes from the second call.

## Features

- Header-only -- zero build configuration, zero dependencies
- EMA (Exponential Moving Average) on **frame time**, not FPS -- eliminates FPS averaging bias (up to 80% error at 30 FPS)
- Time-based smoothing factor -- naturally frame-rate-independent, no `pow()` calls
- Defensive guards: NaN/infinity barrier, min/max dt clamping, floor guard on average
- Deterministically tested via `FakeClock` -- no flaky timing-dependent tests
- `noexcept` and `[[nodiscard]]` on all public methods
- BSL-1.0 licensed -- permissive, no binary attribution

## API Reference

```
template <typename Clock = std::chrono::steady_clock>
class FPSCounter;
```

| Method | Description |
|--------|-------------|
| `explicit FPSCounter(double time_constant = 0.1, double min_dt = 1e-9, double max_dt = 0.1)` | Construct with configurable smoothing parameters |
| `[[nodiscard]] double Update()` | Call once per frame. Returns current FPS. First call returns 0.0 (skip) |
| `[[nodiscard]] double Fps() const` | Last computed FPS without updating. Returns 0.0 before first Update |
| `[[nodiscard]] double FrameTime() const` | Current smoothed frame time in seconds. Returns 0.0 before first Update |
| `void Reset()` | Clear all state, restart measurement. Preserves configuration |

### Parameter details

- `time_constant` -- EMA time constant in seconds (default 0.1). Clamped to >= 1ns.
- `min_dt` -- Minimum frame time clamp in seconds (default 1ns). Clamped to >= 0.
- `max_dt` -- Maximum frame time clamp in seconds (default 0.1s = 10 FPS floor). Clamped to >= min_dt.

### Initial state contract

- `Fps()` before first `Update()` returns `0.0`
- `FrameTime()` before first `Update()` returns `0.0`
- First `Update()` call returns `0.0` (measurement skipped)
- Second `Update()` call returns the first real FPS measurement

## Algorithm Details

```text
1. Compute dt_ns = now - last_time
2. Convert to seconds: dt_s = dt_ns / 1e9
3. NaN/Inf check -> fall back to max_dt
4. Clamp to [min_dt, max_dt]
5. Time-based alpha: alpha = min(dt / tau, 1.0)
6. EMA: avg_dt += alpha * (dt - avg_dt)
7. Floor guard: avg_dt = max(avg_dt, min_dt)
8. Return FPS = 1.0 / avg_dt
```

This is the standard EMA-on-frame-time approach used by Unreal Engine, Bevy, and Dear ImGui.

## CMake Integration

```cmake
# Option A: FetchContent
include(FetchContent)
FetchContent_Declare(fpscounter
    GIT_REPOSITORY https://github.com/user/fpscounter
    GIT_TAG v1.0.0)
FetchContent_MakeAvailable(fpscounter)
target_link_libraries(myapp fpscounter)

# Option B: Copy header
# Copy include/fpscounter/fpscounter.hpp into your project.
# Requires C++17 (std::clamp, [[nodiscard]]).
```

## Portability

| Platform | Status | Notes |
|----------|--------|-------|
| Linux (GCC/Clang) | Full | Tested on GCC 16, Clang 22 |
| Windows (MSVC) | Full | Uses std::chrono::steady_clock |
| macOS (Apple Clang) | Full | Uses std::chrono::steady_clock |
| Emscripten/WASM | Full | Use single thread |
| Android NDK | Full | |
| iOS | Full | |
| CUDA host code | Full | Host code only |
| CUDA device code | Not supported | Use `clock64()` for GPU-side timing |
| Bare-metal/RTOS | Custom clock | Provide a `Clock` implementing `now()` |
| 32-bit ARM soft-float | Slow `double` | Consider `float` template instantiation |

### Compiler flags

| Flag | Effect |
|------|--------|
| `-ffast-math` (GCC/Clang), `/fp:fast` (MSVC) | Breaks IEEE 754 NaN/Inf semantics. `std::isfinite(NaN)` returns `true`, comparisons with NaN produce garbage. The NaN/Inf guards in `Update()` and the constructor become no-ops. A compile-time warning is emitted. **Safe to ignore** if your clock never returns NaN/Inf (true for all standard `<chrono>` clocks). |
| `-fno-exceptions` | Compatible. No exceptions are used. |
| `-fno-rtti` | Compatible. No RTTI is used. |
| `-nostdlib` | Not compatible. Requires `<chrono>`, `<cmath>`, `<algorithm>`. |
| `-std=c++14` or older | Not compatible. Requires C++17 (`std::clamp`, `[[nodiscard]]`). |
| `-mno-sse` (x86, 32-bit) | Compatible. Falls back to x87 FPU for `double`. Slightly slower but correct. |

## Clock Requirements

A valid `Clock` type must provide:

- `Clock::now()` -- static, `noexcept`, returns a `std::chrono::time_point`
- Monotonic behavior (non-decreasing time) -- required for correct measurement
- `duration` convertible to `std::chrono::duration<double>` (for sub-second precision)

The default `std::chrono::steady_clock` satisfies all requirements on all platforms.

## Performance

```
Operation        | Latency (ns)
-----------------|-------------
No-op baseline   | 0.00
Update()         | 31.59
Fps()            | 0.12
Reset()          | 13.05
Constructor      | 13.46
```

The overhead is dominated by `Update()`, which calls `std::chrono::steady_clock::now()` internally. The counter logic itself contributes less than 5 ns per call.

## License

```
Boost Software License - Version 1.0
See accompanying file LICENSE or copy at
https://www.boost.org/LICENSE_1_0.txt
```
