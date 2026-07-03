#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fpscounter/fpscounter.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <ratio>
#include <vector>

struct FakeClock {
  using duration = std::chrono::nanoseconds;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<FakeClock, duration>;
  static constexpr bool kIsSteady = true;

  static std::vector<duration> times;
  static std::size_t index;

  // NOLINTNEXTLINE(readability-identifier-naming)
  static time_point now() noexcept {
    auto ns = times.at(std::min(index++, times.size() - 1));
    return time_point(ns);
  }

  static void Reset() { index = 0; }
};

std::vector<FakeClock::duration> FakeClock::times;
std::size_t FakeClock::index = 0;

using FC = fpscounter::FPSCounter<FakeClock>;

class FakeClockTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FakeClock::times = {std::chrono::nanoseconds(0)};
    FakeClock::index = 0;
  }
  void TearDown() override {
    FakeClock::times.clear();
    FakeClock::index = 0;
  }
};

static void SetUniformDt(std::chrono::nanoseconds dt, std::size_t count = 100) {
  FakeClock::times.clear();
  FakeClock::index = 0;
  FakeClock::times.reserve(count + 1);
  for (std::size_t i = 0; i <= count; ++i) {
    FakeClock::times.push_back(dt * static_cast<int64_t>(i));
  }
}

static void SetTimes(std::initializer_list<FakeClock::duration> ts) {
  FakeClock::times.assign(ts);
  FakeClock::index = 0;
}

static int ConvergenceBudget(
    std::chrono::nanoseconds dt_ns,
    double time_constant,  // NOLINT(bugprone-easily-swappable-parameters)
    double pct) {
  double dt = std::chrono::duration<double>(dt_ns).count();
  double alpha = std::min(dt / time_constant, 1.0);
  if (alpha <= 0.0) {
    return 0;
  }
  // (1-alpha)^n <= pct  →  n >= log(pct) / log(1-alpha)
  return static_cast<int>(std::ceil(std::log(pct) / std::log(1.0 - alpha)));
}

TEST_F(FakeClockTest, FpsReturnsZeroBeforeFirstUpdate) {
  FC counter;
  EXPECT_DOUBLE_EQ(counter.Fps(), 0.0);
  EXPECT_DOUBLE_EQ(counter.FrameTime(), 0.0);
}

TEST_F(FakeClockTest, FirstUpdateReturnsZero) {
  FC counter;
  EXPECT_DOUBLE_EQ(counter.Update(), 0.0);
}

TEST_F(FakeClockTest, SecondUpdateReturnsFiniteFps) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  FC counter;
  (void)counter.Update();
  double fps = counter.Update();
  EXPECT_GT(fps, 0.0);
  EXPECT_TRUE(std::isfinite(fps));
}

TEST_F(FakeClockTest, EmaConvergesAt60Fps) {
  int const kFrames =
      ConvergenceBudget(std::chrono::nanoseconds(16'666'667), 0.1, 0.005);
  SetUniformDt(std::chrono::nanoseconds(16'666'667), kFrames + 2);
  FC counter;

  (void)counter.Update();
  for (int i = 0; i < kFrames; ++i) {
    (void)counter.Update();
  }
  EXPECT_NEAR(counter.Fps(), 60.0, 0.5);
}

TEST_F(FakeClockTest, EmaConvergesAt30Fps) {
  int const kFrames =
      ConvergenceBudget(std::chrono::nanoseconds(33'333'333), 0.1, 0.005);
  SetUniformDt(std::chrono::nanoseconds(33'333'333), kFrames + 2);
  FC counter;
  (void)counter.Update();
  for (int i = 0; i < kFrames; ++i) {
    (void)counter.Update();
  }
  EXPECT_NEAR(counter.Fps(), 30.0, 0.5);
}

TEST_F(FakeClockTest, EmaConvergesAt144Fps) {
  int const kFrames =
      ConvergenceBudget(std::chrono::nanoseconds(6'944'444), 0.1, 0.005);
  SetUniformDt(std::chrono::nanoseconds(6'944'444), kFrames + 2);
  FC counter;
  (void)counter.Update();
  for (int i = 0; i < kFrames; ++i) {
    (void)counter.Update();
  }
  EXPECT_NEAR(counter.Fps(), 144.0, 2.0);
}

TEST_F(FakeClockTest, EmaConvergesAt240Fps) {
  int const kFrames =
      ConvergenceBudget(std::chrono::nanoseconds(4'166'667), 0.1, 0.005);
  SetUniformDt(std::chrono::nanoseconds(4'166'667), kFrames + 2);
  FC counter;
  (void)counter.Update();
  for (int i = 0; i < kFrames; ++i) {
    (void)counter.Update();
  }
  EXPECT_NEAR(counter.Fps(), 240.0, 4.0);
}

TEST_F(FakeClockTest, StepDropFrom60To30) {
  FC counter;

  SetTimes({
      std::chrono::nanoseconds(0),
      std::chrono::nanoseconds(16'666'667),
      std::chrono::nanoseconds(33'333'334),
      std::chrono::nanoseconds(50'000'001),
      std::chrono::nanoseconds(66'666'668),
      std::chrono::nanoseconds(83'333'335),
      std::chrono::nanoseconds(100'000'002),
      std::chrono::nanoseconds(116'666'669),
      std::chrono::nanoseconds(133'333'336),
      std::chrono::nanoseconds(150'000'003),
      std::chrono::nanoseconds(166'666'670),
  });
  (void)counter.Update();
  for (int i = 0; i < 9; ++i) {
    (void)counter.Update();
  }

  for (int i = 0; i < 40; ++i) {
    FakeClock::times.push_back(FakeClock::times.back() +
                               std::chrono::nanoseconds(33'333'333));
  }
  for (int i = 0; i < 40; ++i) {
    (void)counter.Update();
  }

  EXPECT_LT(counter.Fps(), 40.0);
  EXPECT_GT(counter.Fps(), 20.0);
}

TEST_F(FakeClockTest, StepRiseFrom30To120) {
  FC counter;

  SetTimes({
      std::chrono::nanoseconds(0),
      std::chrono::nanoseconds(33'333'333),
      std::chrono::nanoseconds(66'666'666),
      std::chrono::nanoseconds(99'999'999),
      std::chrono::nanoseconds(133'333'332),
      std::chrono::nanoseconds(166'666'665),
      std::chrono::nanoseconds(199'999'998),
      std::chrono::nanoseconds(233'333'331),
      std::chrono::nanoseconds(266'666'664),
      std::chrono::nanoseconds(299'999'997),
      std::chrono::nanoseconds(333'333'330),
  });
  (void)counter.Update();
  for (int i = 0; i < 9; ++i) {
    (void)counter.Update();
  }

  for (int i = 0; i < 40; ++i) {
    FakeClock::times.push_back(FakeClock::times.back() +
                               std::chrono::nanoseconds(8'333'333));
  }
  for (int i = 0; i < 40; ++i) {
    (void)counter.Update();
  }

  EXPECT_GT(counter.Fps(), 90.0);
}

// FakeClock variant with double-based duration that can represent NaN/Inf.
struct DoubleFakeClock {
  using duration = std::chrono::duration<double>;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<DoubleFakeClock, duration>;
  static constexpr bool kIsSteady = true;

  static std::vector<double> seconds;
  static std::size_t index;

  // NOLINTNEXTLINE(readability-identifier-naming)  -- Clock API requires lowercase 'now'
  static time_point now() noexcept {
    double s = seconds.at(std::min(index++, seconds.size() - 1));
    return time_point(duration(s));
  }

  static void Reset() { index = 0; }
};

std::vector<double> DoubleFakeClock::seconds;
std::size_t DoubleFakeClock::index = 0;

using DFC = fpscounter::FPSCounter<DoubleFakeClock>;

class DoubleClockTest : public ::testing::Test {
 protected:
  void SetUp() override {
    DoubleFakeClock::seconds = {0.0};
    DoubleFakeClock::index = 0;
  }
  void TearDown() override {
    DoubleFakeClock::seconds.clear();
    DoubleFakeClock::index = 0;
  }
};

TEST_F(DoubleClockTest, NanDtClampedToMaxDt) {
  DoubleFakeClock::seconds = {0.0, 0.0};
  DFC counter(0.1, 1e-9, 0.1);
  (void)counter.Update();

  DoubleFakeClock::seconds.push_back(std::numeric_limits<double>::quiet_NaN());
  (void)counter.Update();

  EXPECT_NEAR(counter.Fps(), 10.0, 0.1);
}

TEST_F(DoubleClockTest, InfDtClampedToMaxDt) {
  DoubleFakeClock::seconds = {0.0, 0.0};
  DFC counter(0.1, 1e-9, 0.1);
  (void)counter.Update();
  (void)counter.Update();

  DoubleFakeClock::seconds.push_back(std::numeric_limits<double>::infinity());
  (void)counter.Update();

  EXPECT_GT(counter.Fps(), 0.0);
  EXPECT_NEAR(counter.Fps(), 10.0, 0.1);
}

TEST_F(FakeClockTest, NegativeDtClampedToMinDt) {
  SetTimes({
      std::chrono::nanoseconds(1'000'000),
      std::chrono::nanoseconds(500'000),
  });
  FC counter;

  (void)counter.Update();
  (void)counter.Update();
  EXPECT_GT(counter.Fps(), 1.0e8);
}

TEST_F(FakeClockTest, LargeDtClampedToMaxDt) {
  SetTimes({
      std::chrono::nanoseconds(0),
      std::chrono::nanoseconds(1'000'000'000),
      std::chrono::nanoseconds(2'000'000'000),
  });
  FC counter(0.1, 1e-9, 0.1);

  (void)counter.Update();
  (void)counter.Update();
  EXPECT_NEAR(counter.Fps(), 10.0, 0.1);
}

TEST_F(FakeClockTest, TimeConstantClampedToMin) {
  FC counter(0.0);
  EXPECT_DOUBLE_EQ(counter.Fps(), 0.0);

  FC counter2(-1.0);
  EXPECT_DOUBLE_EQ(counter2.Fps(), 0.0);
}

TEST_F(FakeClockTest, MinDtClampedToNonNegative) {
  FC counter(0.1, -5.0);
  EXPECT_DOUBLE_EQ(counter.FrameTime(), 0.0);
}

TEST_F(FakeClockTest, MaxDtClampedToGeMinDt) {
  FC counter(0.1, 0.01, 0.001);
  EXPECT_DOUBLE_EQ(counter.FrameTime(), 0.0);
}

static void ConvergesTo60WithDefaultTc(FC counter) {
  int const kFrames =
      ConvergenceBudget(std::chrono::nanoseconds(16'666'667), 0.1, 0.005);
  (void)counter.Update();
  for (int i = 0; i < kFrames; ++i) {
    (void)counter.Update();
  }
  EXPECT_NEAR(counter.Fps(), 60.0, 0.5);
}

TEST_F(FakeClockTest, NanTimeConstantFallsBackToClamp) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  ConvergesTo60WithDefaultTc(FC(std::numeric_limits<double>::quiet_NaN()));
}

TEST_F(FakeClockTest, InfTimeConstantFallsBackToClamp) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  ConvergesTo60WithDefaultTc(FC(std::numeric_limits<double>::infinity()));
}

TEST_F(FakeClockTest, NanMinDtFallsBackToDefault) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  ConvergesTo60WithDefaultTc(FC(0.1, std::numeric_limits<double>::quiet_NaN()));
}

TEST_F(FakeClockTest, NanMaxDtFallsBackToDefault) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  ConvergesTo60WithDefaultTc(
      FC(0.1, 1e-9, std::numeric_limits<double>::quiet_NaN()));
}

TEST_F(FakeClockTest, InfMinDtFallsBackToDefault) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  ConvergesTo60WithDefaultTc(FC(0.1, std::numeric_limits<double>::infinity()));
}

TEST_F(FakeClockTest, InfMaxDtFallsBackToDefault) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  ConvergesTo60WithDefaultTc(
      FC(0.1, 1e-9, std::numeric_limits<double>::infinity()));
}

TEST_F(FakeClockTest, MinDtZeroProducesFiniteFps) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  FC counter(0.1, 0.0, 0.1);
  (void)counter.Update();
  double fps = counter.Update();
  EXPECT_TRUE(std::isfinite(fps));
  EXPECT_GT(fps, 0.0);
}

TEST_F(FakeClockTest, FloorGuardKeepsAvgDtAboveMinDt) {
  SetTimes({
      std::chrono::nanoseconds(100),
      std::chrono::nanoseconds(100),  // dt = 0
      std::chrono::nanoseconds(100),  // dt = 0
      std::chrono::nanoseconds(100),
  });
  FC counter(0.1, 1e-6, 0.1);

  (void)counter.Update();
  (void)counter.Update();
  EXPECT_GE(counter.FrameTime(), 1e-6);

  (void)counter.Update();
  EXPECT_NEAR(counter.FrameTime(), 1e-6, 1e-12);
}

TEST_F(FakeClockTest, ResetRestartsMeasurement) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  FC counter;

  (void)counter.Update();
  (void)counter.Update();
  EXPECT_GT(counter.Fps(), 0.0);

  counter.Reset();
  EXPECT_DOUBLE_EQ(counter.Fps(), 0.0);
  EXPECT_DOUBLE_EQ(counter.FrameTime(), 0.0);
  EXPECT_DOUBLE_EQ(counter.Update(), 0.0);
}

TEST_F(FakeClockTest, ResetPreservesConfig) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  FC counter(0.5, 1e-8, 0.2);
  counter.Reset();

  EXPECT_DOUBLE_EQ(counter.Fps(), 0.0);
  EXPECT_DOUBLE_EQ(counter.Update(), 0.0);
}

TEST_F(FakeClockTest, CopyConstructible) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  FC counter;
  (void)counter.Update();
  (void)counter.Update();
  FC copy(counter);
  EXPECT_DOUBLE_EQ(copy.Fps(), counter.Fps());
}

TEST_F(FakeClockTest, MoveConstructible) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  FC counter;
  (void)counter.Update();
  (void)counter.Update();
  double fps = counter.Fps();
  FC moved(counter);
  // Moved-to object retains the FPS state
  EXPECT_DOUBLE_EQ(moved.Fps(), fps);
  // Moved-to object is still fully functional
  (void)moved.Update();
  EXPECT_GT(moved.Fps(), 0.0);
}

TEST(RealClockTest, HighResClock) {
  using HRC = fpscounter::FPSCounter<std::chrono::high_resolution_clock>;
  HRC counter;
  EXPECT_DOUBLE_EQ(counter.Update(), 0.0);
  double fps = counter.Update();
  EXPECT_GT(fps, 0.0);
  EXPECT_TRUE(std::isfinite(fps));
}

TEST(RealClockTest, SystemClock) {
  using SC = fpscounter::FPSCounter<std::chrono::system_clock>;
  SC counter;
  EXPECT_DOUBLE_EQ(counter.Update(), 0.0);
  double fps = counter.Update();
  EXPECT_GT(fps, 0.0);
  EXPECT_TRUE(std::isfinite(fps));
}

TEST_F(FakeClockTest, VeryShortTimeConstantTracksQuickly) {
  SetUniformDt(std::chrono::nanoseconds(16'666'667));
  FC counter(1e-9);

  (void)counter.Update();
  (void)counter.Update();
  EXPECT_NEAR(counter.Fps(), 60.0, 0.1);
}

TEST_F(FakeClockTest, VeryLongTimeConstantAdaptsSlowly) {
  FC counter(10.0);

  SetTimes({
      std::chrono::nanoseconds(0),
      std::chrono::nanoseconds(16'666'667),
      std::chrono::nanoseconds(33'333'334),
      std::chrono::nanoseconds(50'000'001),
      std::chrono::nanoseconds(66'666'668),
      std::chrono::nanoseconds(83'333'335),
  });
  (void)counter.Update();
  for (int i = 0; i < 4; ++i) {
    (void)counter.Update();
  }

  EXPECT_GT(counter.Fps(), 60.0);
}

TEST_F(FakeClockTest, MultipleInstancesIndependent) {
  FC counter1;
  FC counter2;

  FakeClock::times.clear();
  for (int i = 0; i <= 20; ++i) {
    FakeClock::times.emplace_back(i * 16'666'667);
  }
  FakeClock::index = 0;

  for (int i = 0; i < 10; ++i) {
    (void)counter1.Update();
    (void)counter2.Update();
  }

  EXPECT_NEAR(counter1.Fps(), counter2.Fps(), 1.0);
}

TEST_F(FakeClockTest, ManyConsecutiveUpdatesStable) {
  int const kFrames = 10'000;
  SetUniformDt(std::chrono::nanoseconds(16'666'667), kFrames);

  FC counter;
  (void)counter.Update();
  for (int i = 0; i < kFrames - 1; ++i) {
    (void)counter.Update();
  }

  EXPECT_NEAR(counter.Fps(), 60.0, 0.5);
  EXPECT_GT(counter.FrameTime(), 0.0);
  EXPECT_TRUE(std::isfinite(counter.Fps()));
}

TEST_F(FakeClockTest, MaxDtOneEnforcesOneFpsFloor) {
  SetTimes({
      std::chrono::nanoseconds(0),
      std::chrono::nanoseconds(5'000'000'000),
      std::chrono::nanoseconds(10'000'000'000),
  });
  FC counter(0.1, 1e-9, 1.0);

  (void)counter.Update();
  (void)counter.Update();
  EXPECT_NEAR(counter.Fps(), 1.0, 0.01);
}

TEST_F(FakeClockTest, MethodsAreNoexcept) {
  FC counter;
  EXPECT_TRUE(noexcept(counter.Update()));
  EXPECT_TRUE(noexcept(counter.Fps()));
  EXPECT_TRUE(noexcept(counter.FrameTime()));
  EXPECT_TRUE(noexcept(counter.Reset()));
}

TEST_F(FakeClockTest, BurstyFrameTimesProduceFiniteResults) {
  SetTimes({
      std::chrono::nanoseconds(0),
      std::chrono::nanoseconds(1'000'000),
      std::chrono::nanoseconds(50'000'000),
      std::chrono::nanoseconds(51'000'000),
      std::chrono::nanoseconds(100'000'000),
  });

  FC counter;
  (void)counter.Update();
  double fps1 = counter.Update();
  double fps2 = counter.Update();
  double fps3 = counter.Update();
  double fps4 = counter.Update();

  EXPECT_TRUE(std::isfinite(fps1));
  EXPECT_TRUE(std::isfinite(fps2));
  EXPECT_TRUE(std::isfinite(fps3));
  EXPECT_TRUE(std::isfinite(fps4));
  EXPECT_GT(fps1, 0.0);
  EXPECT_GT(fps2, 0.0);
  EXPECT_GT(fps3, 0.0);
  EXPECT_GT(fps4, 0.0);
}

TEST_F(FakeClockTest, ZeroDtHandledGracefully) {
  SetTimes({
      std::chrono::nanoseconds(1000),
      std::chrono::nanoseconds(1000),
      std::chrono::nanoseconds(1000),
  });
  FC counter;

  (void)counter.Update();
  (void)counter.Update();
  double fps = counter.Update();
  EXPECT_TRUE(std::isfinite(fps));
  EXPECT_GT(fps, 0.0);
}

struct FloatClock {
  using duration = std::chrono::duration<float, std::nano>;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<FloatClock, duration>;
  static constexpr bool kIsSteady = true;

  static float now_val;

  // NOLINTNEXTLINE(readability-identifier-naming)  -- Clock API requires lowercase 'now'
  static time_point now() noexcept {
    float v = now_val;
    now_val += 16'666'667.0f;
    return time_point(duration(v));
  }

  static void Reset() { now_val = 0.0f; }
};

float FloatClock::now_val = 0.0f;

class FloatClockTest : public ::testing::Test {
 protected:
  void SetUp() override { FloatClock::Reset(); }
  void TearDown() override { FloatClock::Reset(); }
};

TEST_F(FloatClockTest, Instantiation) {
  using FloatFC = fpscounter::FPSCounter<FloatClock>;
  FloatFC counter;
  EXPECT_DOUBLE_EQ(counter.Update(), 0.0);
  double fps = counter.Update();
  EXPECT_GT(fps, 0.0);
  EXPECT_TRUE(std::isfinite(fps));
}
