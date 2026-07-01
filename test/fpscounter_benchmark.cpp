#include <fpscounter/fpscounter.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <ratio>
#include <vector>

class Timer {
 public:
  using Clock = std::chrono::high_resolution_clock;

  void Start() { start_ = Clock::now(); }
  void Stop() { elapsed_ = Clock::now() - start_; }

  [[nodiscard]] double Seconds() const {
    return std::chrono::duration<double>(elapsed_).count();
  }
  [[nodiscard]] double Nanoseconds() const {
    return std::chrono::duration<double, std::nano>(elapsed_).count();
  }

 private:
  Clock::time_point start_;
  Clock::duration elapsed_{};
};

struct Result {
  const char* name_;
  double ns_per_call_;
  double calls_per_sec_;
};

static constexpr int64_t kTrials = 5;
static constexpr int64_t kWarmup = 1000000;
static constexpr int64_t kIterations = 10000000;

template <typename Fn>
static double Bench(Fn fn, int64_t n) {
  for (int64_t i = 0; i < kWarmup; ++i) {
    fn();
  }

  std::vector<double> trials;
  trials.reserve(kTrials);

  for (int64_t t = 0; t < kTrials; ++t) {
    Timer timer;
    timer.Start();
    for (int64_t i = 0; i < n; ++i) {
      fn();
    }
    timer.Stop();
    double ns = timer.Nanoseconds() / static_cast<double>(n);
    trials.push_back(ns);
  }

  std::sort(trials.begin(), trials.end());
  return trials.at(kTrials / 2);
}

struct NoopBaseline {
  using Clock = std::chrono::steady_clock;
  Clock::time_point last_{Clock::now()};

  double Tick() {
    auto now = Clock::now();
    double dt = std::chrono::duration<double>(now - last_).count();
    last_ = now;
    // Simulate the compute but cheaply
    volatile double sink = dt;  // NOLINT
    (void)sink;
    return 0.0;
  }
};

int main() {
  using FPSCounter = fpscounter::FPSCounter<std::chrono::steady_clock>;

  std::cout << "╔══════════════════════════════════════════════════════════╗\n";
  std::cout << "║    fpscounter — Microbenchmark                          ║\n";
  std::cout << "╚══════════════════════════════════════════════════════════╝\n";
  std::cout << "\n";
#if defined(_MSC_VER)
  std::cout << "  Compiler:  MSVC " << _MSC_VER << "\n";
#elif defined(__clang__)
  std::cout << "  Compiler:  Clang " << __clang_version__ << "\n";
#else
  std::cout << "  Compiler:  GCC " << __VERSION__ << "\n";
#endif
  std::cout << "  Standard:  C++" << __cplusplus << "\n";
  std::cout << "  Iterations per trial: " << kIterations << "  (" << kTrials
            << " trials, median reported)\n";
  std::cout << "  Warmup:    " << kWarmup << "\n";
  std::cout << "\n";

  std::vector<Result> results;

  {
    NoopBaseline noop;
    double ns = Bench([&] { noop.Tick(); }, kIterations);
    results.push_back({"No-op baseline (loop + now())", ns, 1e9 / ns});
  }

  {
    double ns = Bench(
        [] {
          FPSCounter c;
          (void)c;
        },
        kIterations / 10);
    results.push_back({"Constructor", ns, 1e9 / ns});
  }

  {
    double ns = Bench(
        [] {
          static FPSCounter c;
          (void)c.Update();
        },
        kIterations);
    results.push_back({"Update()", ns, 1e9 / ns});
  }

  {
    FPSCounter c;
    (void)c.Update();
    (void)c.Update();
    double ns = Bench([&] { (void)c.Fps(); }, kIterations);
    results.push_back({"Fps()", ns, 1e9 / ns});
  }

  {
    FPSCounter c;
    (void)c.Update();
    (void)c.Update();
    double ns = Bench([&] { (void)c.FrameTime(); }, kIterations);
    results.push_back({"FrameTime()", ns, 1e9 / ns});
  }

  {
    FPSCounter c;
    (void)c.Update();
    double ns = Bench([&] { c.Reset(); }, kIterations / 10);
    results.push_back({"Reset()", ns, 1e9 / ns});
  }

  std::cout << "  Operation                  │  Latency (ns)  │  Calls/sec\n";
  std::cout
      << "  ───────────────────────────┼────────────────┼──────────────\n";
  for (auto const& r : results) {
    std::cout << "  " << std::left << std::setw(27) << r.name_ << " │ "
              << std::right << std::setw(11) << std::fixed
              << std::setprecision(2) << r.ns_per_call_ << "  │ "
              << std::setw(10) << std::fixed << std::setprecision(0)
              << r.calls_per_sec_ << "\n";
  }
  std::cout << "\n";

  // Sanity: Update should not be >5x slower than the baseline
  double baseline_ns = results.at(0).ns_per_call_;
  double update_ns = results.at(2).ns_per_call_;
  if (update_ns > baseline_ns * 5.0) {
    std::cerr << "WARNING: Update() latency seems high (" << update_ns
              << " ns vs baseline " << baseline_ns << " ns)\n";
    return 1;
  }

  std::cout << "  ✓ All benchmarks passed.\n";
  return 0;
}
