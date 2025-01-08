#include <benchmark/benchmark.h>

#include "cc/core/utils/src/http.h"

namespace google::scp::core::utils {
namespace {

void BM_ExtractValidUserAgent(benchmark::State& state) {
  HttpHeaders request_headers;
  request_headers.insert({"user-agent", "aggregation-service/2.5.0"});

  for (auto _ : state) {
    auto user_agent = ExtractUserAgent(request_headers);
    benchmark::DoNotOptimize(user_agent);
  }
}

void BM_ExtractInvalidUserAgent(benchmark::State& state) {
  core::HttpHeaders request_headers;
  request_headers.insert({"user-agent", "some-other-service/2.5.0"});

  for (auto _ : state) {
    auto user_agent = ExtractUserAgent(request_headers);
    benchmark::DoNotOptimize(user_agent);
  }
}

BENCHMARK(BM_ExtractValidUserAgent);
BENCHMARK(BM_ExtractInvalidUserAgent);

}  // namespace
}  // namespace google::scp::core::utils

// Run the benchmark.
BENCHMARK_MAIN();
