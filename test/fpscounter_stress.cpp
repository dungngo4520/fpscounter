#include <algorithm>
#include <fpscounter/fpscounter.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::mutex g_report_mutex;

struct StressResult {
  std::string name_;
  bool passed_;
  std::string detail_;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::vector<StressResult> g_results;

static void Record(const std::string& name, bool passed,
                   const std::string& detail = "") {
  std::scoped_lock<std::mutex> lock(g_report_mutex);
  g_results.push_back({name, passed, detail});
}

static void TestMassiveUpdates() {
  using FPSCounter = fpscounter::FPSCounter<std::chrono::steady_clock>;
  FPSCounter counter;

  (void)counter.Update();

  int64_t constexpr kUpdates = 10'000'000;
  auto start = std::chrono::steady_clock::now();

  for (int64_t i = 0; i < kUpdates; ++i) {
    double fps = counter.Update();
    if (!std::isfinite(fps)) {
      Record("10M Updates", false,
             "Non-finite FPS at iteration " + std::to_string(i) + ": " +
                 std::to_string(fps));
      return;
    }
  }

  auto end = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(end - start).count();
  double fps_now = counter.Fps();

  bool pass = std::isfinite(fps_now) && fps_now > 0.0;
  std::ostringstream ss;
  ss << kUpdates << " updates in " << std::fixed << std::setprecision(2)
     << elapsed << "s (" << std::fixed << std::setprecision(0)
     << (kUpdates / elapsed) << " updates/sec), final FPS=" << std::fixed
     << std::setprecision(1) << fps_now;
  Record("10M Updates", pass, ss.str());
}

static void TestMultiThreaded() {
  using FPSCounter = fpscounter::FPSCounter<std::chrono::steady_clock>;
  int constexpr kThreads = 8;
  int64_t constexpr kUpdatesPerThread = 2'000'000;

  std::atomic<bool> any_failed{false};

  auto worker = [&](int /*id*/) {
    FPSCounter counter;
    (void)counter.Update();

    for (int64_t i = 0; i < kUpdatesPerThread; ++i) {
      double fps = counter.Update();
      if (!std::isfinite(fps)) {
        any_failed.store(true);
        return;
      }
    }
  };

  auto start = std::chrono::steady_clock::now();

  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back(worker, i);
  }
  for (auto& t : threads) {
    t.join();
  }

  auto end = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(end - start).count();

  double total =
      static_cast<double>(kThreads) * static_cast<double>(kUpdatesPerThread);
  std::ostringstream ss;
  ss << kThreads << " threads × " << kUpdatesPerThread << " updates in "
     << std::fixed << std::setprecision(2) << elapsed << "s (" << std::fixed
     << std::setprecision(0) << (total / elapsed) << " updates/sec)";

  Record("Multi-threaded (" + std::to_string(kThreads) + " threads)",
         !any_failed.load(), ss.str());
}

static void TestMinDtRapidFire() {
  // Use a variant with very short time_constant to keep the test quick.
  // We stress the NaN/Inf clamps and min_dt floor at maximum rate.
  using FPSCounter = fpscounter::FPSCounter<std::chrono::steady_clock>;
  int64_t constexpr kUpdates = 5'000'000;

  FPSCounter counter(0.001, 1e-9, 0.001);

  (void)counter.Update();

  for (int64_t i = 0; i < kUpdates; ++i) {
    double fps = counter.Update();
    if (!std::isfinite(fps)) {
      Record("Min-dt rapid-fire", false,
             "Non-finite FPS at iteration " + std::to_string(i));
      return;
    }
  }

  Record("Min-dt rapid-fire", true,
         std::to_string(kUpdates) + " updates at max rate, all finite");
}

static void TestExtremeAlternation() {
  using FPSCounter = fpscounter::FPSCounter<std::chrono::steady_clock>;
  int64_t constexpr kCycles = 100'000;

  FPSCounter counter(0.01, 1e-9, 1.0);

  (void)counter.Update();

  for (int64_t i = 0; i < kCycles; ++i) {
    double fps = counter.Update();
    if (!std::isfinite(fps)) {
      Record("Extreme dt alternation", false,
             "Non-finite at iteration " + std::to_string(i));
      return;
    }

    volatile double sink = 0.0;
    for (int b = 0; b < (i % 2 == 0 ? 10 : 1000); ++b) {
      sink += static_cast<double>(b) * 0.5;
    }
    (void)sink;
  }

  double fps = counter.Fps();
  bool pass = std::isfinite(fps) && fps > 0.0;
  Record("Extreme dt alternation", pass, "Final FPS: " + std::to_string(fps));
}

static void TestLongRunningStability() {
  using FPSCounter = fpscounter::FPSCounter<std::chrono::steady_clock>;
  FPSCounter counter;
  (void)counter.Update();

  auto start = std::chrono::steady_clock::now();
  auto deadline = start + std::chrono::seconds(1);

  int64_t frame_count = 0;
  double min_fps = std::numeric_limits<double>::max();
  double max_fps = 0.0;

  while (std::chrono::steady_clock::now() < deadline) {
    double fps = counter.Update();
    ++frame_count;
    if (std::isfinite(fps)) {
      min_fps = std::min(min_fps, fps);
      max_fps = std::max(max_fps, fps);
    } else {
      Record("Long-running stability (1s)", false,
             "Non-finite FPS at frame " + std::to_string(frame_count));
      return;
    }
  }

  auto end = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(end - start).count();

  std::ostringstream ss;
  ss << frame_count << " frames in " << std::fixed << std::setprecision(2)
     << elapsed << "s, FPS range: " << std::fixed << std::setprecision(1)
     << min_fps << " – " << max_fps;
  Record("Long-running stability (1s)", true, ss.str());
}

static void TestRepeatedConstruction() {
  using FPSCounter = fpscounter::FPSCounter<std::chrono::steady_clock>;
  int64_t constexpr kConstructors = 1'000'000;

  auto start = std::chrono::steady_clock::now();
  for (int64_t i = 0; i < kConstructors; ++i) {
    FPSCounter c;
    (void)c.Update();
    (void)c.Fps();
  }
  auto end = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(end - start).count();

  std::ostringstream ss;
  ss << kConstructors << " construct+update+query in " << std::fixed
     << std::setprecision(2) << elapsed << "s (" << std::fixed
     << std::setprecision(0) << (kConstructors / elapsed) << " cycles/sec)";
  Record("Repeated construction", true, ss.str());
}

static void TestAggressivePingPong() {
  // Continuously create, update, reset, destroy in tight loop.
  using FPSCounter = fpscounter::FPSCounter<std::chrono::steady_clock>;
  int64_t constexpr kCycles = 500'000;

  for (int64_t i = 0; i < kCycles; ++i) {
    FPSCounter c;
    (void)c.Update();
    double f1 = c.Update();
    if (!std::isfinite(f1)) {
      Record("Aggressive ping-pong", false,
             "Non-finite at cycle " + std::to_string(i));
      return;
    }
    c.Reset();
    double f2 = c.Update();
    if (f2 != 0.0) {
      Record("Aggressive ping-pong", false,
             "Reset broken at cycle " + std::to_string(i));
      return;
    }
  }

  Record("Aggressive ping-pong", true,
         std::to_string(kCycles) + " reset cycles");
}

int main() {
  std::cout << "╔══════════════════════════════════════════════════════════╗\n";
  std::cout << "║    fpscounter — Stress Tests                            ║\n";
  std::cout << "╚══════════════════════════════════════════════════════════╝\n";
  std::cout << "\n";
#if defined(_MSC_VER)
  std::cout << "  Compiler: MSVC " << _MSC_VER << "\n";
#elif defined(__clang__)
  std::cout << "  Compiler: Clang " << __clang_version__ << "\n";
#else
  std::cout << "  Compiler: GCC " << __VERSION__ << "\n";
#endif
  std::cout << "  Standard: C++" << __cplusplus << "\n";
  std::cout << "  Threads:  " << std::thread::hardware_concurrency()
            << " logical cores\n";
  std::cout << "\n";

  struct Test {
    const char* name_;
    void (*fn_)();
  };

  std::array<Test, 7> tests = {{
      {"10M consecutive Update() calls", TestMassiveUpdates},
      {"Multi-threaded (8 threads)", TestMultiThreaded},
      {"Min-dt rapid-fire", TestMinDtRapidFire},
      {"Extreme dt alternation", TestExtremeAlternation},
      {"Long-running stability (1s)", TestLongRunningStability},
      {"Repeated construction", TestRepeatedConstruction},
      {"Aggressive ping-pong", TestAggressivePingPong},
  }};

  int passed = 0;
  int failed = 0;

  for (auto const& t : tests) {
    std::cout << "  ◉ " << t.name_ << "...\n";
    t.fn_();
  }

  std::cout << "\n";
  std::cout << "  ── Results ───────────────────────────────────────\n";
  for (auto const& r : g_results) {
    std::cout << "  " << (r.passed_ ? "✓" : "✗") << " " << r.name_ << "\n";
    if (!r.detail_.empty()) {
      std::cout << "      " << r.detail_ << "\n";
    }
    if (r.passed_) {
      ++passed;
    } else {
      ++failed;
    }
  }

  std::cout << "\n";
  std::cout << "  " << (passed + failed) << " tests: " << passed << " passed"
            << (failed > 0 ? ", " + std::to_string(failed) + " FAILED" : "")
            << "\n";
  std::cout << "\n";

  return failed > 0 ? 1 : 0;
}
