// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cmath>
#include <string>
#include <thread>

#include <benchmark/benchmark.h>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/blocking_counter.h"
#include "cc/core/async_executor/src/async_executor.h"

namespace privacy_sandbox::pbs_common {
namespace {

absl::Duration ComputePercentile(absl::Span<const absl::Duration> duration_list,
                                 float percentile) {
  if (duration_list.empty()) {
    return absl::InfiniteDuration();
  }
  int percentile_index =
      std::ceil(duration_list.size() * percentile / 100.0) - 1;
  return duration_list[percentile_index];
}

class ExecutorFixture : public benchmark::Fixture {
 public:
  void SetUp(benchmark::State& state) {
    executor_ = std::make_unique<AsyncExecutor>(
        std::thread::hardware_concurrency(), 1000);
    executor_->Init();
    executor_->Run();
  }

  void TearDown(benchmark::State& state) {
    executor_->Stop();
    std::vector<absl::Duration> all_latencies;
    for (const auto& [tid, latencies] :
         executor_->SchedulingLatencyPerThreadForTesting()) {
      all_latencies.insert(all_latencies.end(), latencies.begin(),
                           latencies.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    state.counters[absl::StrCat("p50")] =
        absl::ToInt64Microseconds(ComputePercentile(all_latencies, 50.0));
    state.counters[absl::StrCat("p99")] =
        absl::ToInt64Microseconds(ComputePercentile(all_latencies, 99.0));
  }

  std::unique_ptr<AsyncExecutor> executor_;
};

BENCHMARK_DEFINE_F(ExecutorFixture, Schedule)(benchmark::State& state) {
  for (const auto& _ : state) {
    absl::BlockingCounter counter(state.range(0));
    for (int i = 0; i < state.range(0); ++i) {
      AsyncOperation operation = AsyncOperation([&]() {
        int first = 0, second = 1, next;

        for (int i = 0; i < 100000000; ++i) {
          next = first + second % 1000;
          first = second;
          second = next;
        }
        counter.DecrementCount();
      });
      ExecutionResult result =
          executor_->Schedule(operation, AsyncPriority::Normal);
      if (!result.Successful()) {
        state.SkipWithError("Failed to schedule!");
      }
    }
    counter.Wait();
  }
}

// Register the function as a benchmark.
BENCHMARK_REGISTER_F(ExecutorFixture, Schedule)->Range(1, 1 << 19);

}  // namespace
}  // namespace privacy_sandbox::pbs_common

// Run the benchmark.
BENCHMARK_MAIN();
