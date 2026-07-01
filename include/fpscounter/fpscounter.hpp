#ifndef FPSCOUNTER_FPSCOUNTER_HPP
#define FPSCOUNTER_FPSCOUNTER_HPP

#include <algorithm>
#include <chrono>
#include <cmath>

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
    : inv_time_constant_(1.0 / std::max(time_constant, 1e-9)),
      min_dt_(std::max(min_dt, 0.0)),
      max_dt_(max_dt),
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

#endif  // FPSCOUNTER_FPSCOUNTER_HPP
