option(FPSCOUNTER_BUILD_TESTS "Build tests" ON)
option(FPSCOUNTER_BUILD_BENCHMARK "Build benchmark" ON)
option(FPSCOUNTER_BUILD_STRESS "Build stress tests" ON)

# ── Unit Tests (Google Test) ─────────────────────────────────────────────────
if(FPSCOUNTER_BUILD_TESTS)
  find_package(GTest REQUIRED)

  add_executable(fpscounter_test fpscounter_test.cpp)
  target_link_libraries(fpscounter_test PRIVATE fpscounter GTest::gtest GTest::gtest_main)
  target_compile_features(fpscounter_test PRIVATE cxx_std_17)

  add_test(NAME fpscounter_test COMMAND fpscounter_test)
endif()

# ── Benchmark ────────────────────────────────────────────────────────────────
if(FPSCOUNTER_BUILD_BENCHMARK)
  add_executable(fpscounter_benchmark fpscounter_benchmark.cpp)
  target_link_libraries(fpscounter_benchmark PRIVATE fpscounter)
  target_compile_features(fpscounter_benchmark PRIVATE cxx_std_17)

  add_test(NAME fpscounter_benchmark COMMAND fpscounter_benchmark)
endif()

# ── Stress Tests ─────────────────────────────────────────────────────────────
if(FPSCOUNTER_BUILD_STRESS)
  find_package(Threads REQUIRED)
  add_executable(fpscounter_stress fpscounter_stress.cpp)
  target_link_libraries(fpscounter_stress PRIVATE fpscounter Threads::Threads)
  target_compile_features(fpscounter_stress PRIVATE cxx_std_17)

  add_test(NAME fpscounter_stress COMMAND fpscounter_stress)
endif()
