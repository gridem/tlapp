#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

struct BenchResult {
  std::string name;
  size_t iterations = 0;
  uint64_t checksum = 0;
  double totalMs = 0.0;
  double perIterUs = 0.0;
};

template <typename F>
BenchResult runBench(std::string_view name, size_t iterations, F&& f) {
  for (size_t i = 0; i < 3; ++i) {
    (void)f();
  }

  uint64_t checksum = 0;
  auto start = std::chrono::steady_clock::now();
  for (size_t i = 0; i < iterations; ++i) {
    checksum += static_cast<uint64_t>(f());
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
