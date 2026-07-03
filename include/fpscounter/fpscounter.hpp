#ifndef FPSCOUNTER_FPSCOUNTER_HPP
#define FPSCOUNTER_FPSCOUNTER_HPP

#include <algorithm>
#include <chrono>
#include <cmath>

// ── Fast-math detection ──────────────────────────────────────────────────────
// -ffast-math (GCC/Clang), -ffinite-math-only (GCC/Clang), and /fp:fast (MSVC)
// break IEEE 754 NaN/Inf semantics: std::isfinite returns true for NaN,
// comparisons with NaN produce garbage, and 1.0/NaN returns 0 instead of NaN.
// This disables the NaN/Inf guards in Update() and the constructor.
// Safe to ignore if your clock never returns NaN/Inf (true for all standard
// <chrono> clocks).
#if defined(__FAST_MATH__) ||                                  \
    (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__) || \
    defined(_M_FP_FAST)
#if defined(_MSC_VER)
#pragma message( \
    "fpscounter: /fp:fast detected -- NaN/Inf guards disabled. Safe if clock never returns NaN/Inf. See README.")
#else
#warning \
    "fpscounter: -ffast-math detected -- NaN/Inf guards disabled. Safe if clock never returns NaN/Inf. See README."
#endif
#endif

// Suppress clang's redundant -Wnan-infinity-disabled for our std::isfinite calls.
// Our custom #warning above already explains the situation better.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnan-infinity-disabled"
#endif

namespace fpscounter {

template <typename Clock = std::chrono::steady_clock>
class FPSCounter {
 public:
  using ClockType = Clock;
  using TimePoint = typename Clock::time_point;

  explicit FPSCounter(double time_constant = 0.1, double min_dt = 1e-9,
                      double max_dt = 0.1) noexcept;

  [[nodiscard]] double Update() noexcept;
  [[nodiscard]] double Fps() const noexcept;
  [[nodiscard]] double FrameTime() const noexcept;
  void Reset() noexcept;

 private:
  double inv_time_constant_;
  double min_dt_;
  double max_dt_;
  double avg_dt_;
  TimePoint last_time_;
  bool first_frame_;
};

template <typename Clock>
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
FPSCounter<Clock>::FPSCounter(double time_constant, double min_dt,
                              double max_dt) noexcept
    : inv_time_constant_(
          1.0 /
          std::max(std::isfinite(time_constant) ? time_constant : 0.1, 1e-9)),
      min_dt_(std::isfinite(min_dt) ? std::max(min_dt, 0.0) : 1e-9),
      max_dt_(std::isfinite(max_dt) ? max_dt : 0.1),
      avg_dt_(min_dt_),
      last_time_(Clock::now()),
      first_frame_(true) {
  // Enforce: max_dt >= min_dt.
  // Done in body to avoid C++ member init-order dependency between max_dt_ and
  // min_dt_ (members initialize in declaration order, not initializer-list
  // order).
  max_dt_ = std::max(max_dt_, min_dt_);
}

template <typename Clock>
double FPSCounter<Clock>::Update() noexcept {
  auto now = Clock::now();
  if (first_frame_) {
    last_time_ = now;
    first_frame_ = false;
    return 0.0;
  }

  double dt = std::chrono::duration<double>(now - last_time_).count();
  last_time_ = now;

  if (!std::isfinite(dt)) {
    dt = max_dt_;
  } else {
    dt = std::clamp(dt, min_dt_, max_dt_);
  }

  double alpha = std::min(dt * inv_time_constant_, 1.0);
  avg_dt_ += alpha * (dt - avg_dt_);
  avg_dt_ = std::max(avg_dt_, min_dt_);

  return 1.0 / avg_dt_;
}

template <typename Clock>
double FPSCounter<Clock>::Fps() const noexcept {
  return first_frame_ ? 0.0 : 1.0 / avg_dt_;
}

template <typename Clock>
double FPSCounter<Clock>::FrameTime() const noexcept {
  return first_frame_ ? 0.0 : avg_dt_;
}

template <typename Clock>
void FPSCounter<Clock>::Reset() noexcept {
  last_time_ = Clock::now();
  avg_dt_ = min_dt_;
  first_frame_ = true;
}

}  // namespace fpscounter

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif  // FPSCOUNTER_FPSCOUNTER_HPP
