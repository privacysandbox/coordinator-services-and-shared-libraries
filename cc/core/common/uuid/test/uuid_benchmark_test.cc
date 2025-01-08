#include <cmath>

#include <benchmark/benchmark.h>

#include "cc/core/common/uuid/src/uuid.h"

namespace google::scp::core::common {
namespace {

static void BM_UuidFromString(benchmark::State& state) {
  std::string uuid = ToString(Uuid::GenerateUuid());
  Uuid uuid_result;
  for (const auto& _ : state) {
    for (int i = 0; i < state.range(0); ++i) {
      benchmark::DoNotOptimize(FromString(uuid, uuid_result));
    }
  }
}

static void BM_UuidToString(benchmark::State& state) {
  auto uuid = Uuid::GenerateUuid();
  Uuid uuid_result;
  for (const auto& _ : state) {
    for (int i = 0; i < state.range(0); ++i) {
      benchmark::DoNotOptimize(ToString(uuid));
    }
  }
}

// Register the function as a benchmark.
BENCHMARK(BM_UuidFromString)->Range(1, 1 << 19);
BENCHMARK(BM_UuidToString)->Range(1, 1 << 19);

}  // namespace
}  // namespace google::scp::core::common

// Run the benchmark.
BENCHMARK_MAIN();
