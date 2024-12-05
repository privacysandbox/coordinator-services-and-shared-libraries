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
      FromString(uuid, uuid_result);
    }
  }
}

// Register the function as a benchmark.
BENCHMARK(BM_UuidFromString)->Range(1, 1 << 19);

}  // namespace
}  // namespace google::scp::core::common

// Run the benchmark.
BENCHMARK_MAIN();
