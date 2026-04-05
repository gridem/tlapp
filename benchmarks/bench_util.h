#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "engine.h"

struct BenchConfig {
  size_t warmupIterations = 3;
};

struct BenchResult {
  std::string name;
  size_t iterations = 0;
  uint64_t checksum = 0;
  double totalMs = 0.0;
  double perIterUs = 0.0;
};

inline uint64_t benchValue(bool value) { return value ? 1ull : 0ull; }

inline uint64_t benchValue(const BooleanResult& result) {
  if (auto logic = std::get_if<LogicResult>(&result)) {
    return logic->size();
  }
  return benchValue(std::get<bool>(result));
}

inline uint64_t benchValue(const Stats& stats) {
  return static_cast<uint64_t>(stats.loop.states + stats.loop.transitions);
}

template <typename T>
uint64_t benchValue(T value) {
  return static_cast<uint64_t>(value);
}

template <typename F>
BenchResult runBench(std::string_view name, size_t iterations, F&& f,
                     BenchConfig config = {}) {
  for (size_t i = 0; i < config.warmupIterations; ++i) {
    (void)benchValue(f());
  }

  uint64_t checksum = 0;
  auto start = std::chrono::steady_clock::now();
  for (size_t i = 0; i < iterations; ++i) {
    checksum += benchValue(f());
  }
  auto finish = std::chrono::steady_clock::now();

  auto elapsed = std::chrono::duration<double, std::milli>(finish - start);
  auto perIterUs = std::chrono::duration<double, std::micro>(finish - start);
  perIterUs /= static_cast<double>(iterations);

  BenchResult result{std::string{name}, iterations, checksum, elapsed.count(),
                     perIterUs.count()};

  std::cout << std::fixed << std::setprecision(3) << "BENCH name=" << result.name
            << " iterations=" << result.iterations
            << " checksum=" << result.checksum << " total_ms=" << result.totalMs
            << " per_iter_us=" << result.perIterUs << '\n';
  return result;
}

template <typename F>
BenchResult expectBenchChecksum(std::string_view name, size_t iterations,
                                uint64_t expectedChecksum, F&& f,
                                BenchConfig config = {}) {
  auto result = runBench(name, iterations, std::forward<F>(f), config);
  EXPECT_EQ(expectedChecksum, result.checksum);
  return result;
}

template <typename F>
BenchResult expectBenchPerIteration(std::string_view name, size_t iterations,
                                    uint64_t expectedPerIteration, F&& f,
                                    BenchConfig config = {}) {
  return expectBenchChecksum(name, iterations,
                             expectedPerIteration * iterations,
                             std::forward<F>(f), config);
}
