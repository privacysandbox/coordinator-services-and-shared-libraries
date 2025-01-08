// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include <benchmark/benchmark.h>

#include "cc/pbs/budget_key_timeframe_manager/src/budget_key_timeframe_serialization.h"

namespace google::scp::pbs::budget_key_timeframe_manager {
namespace {

void BM_DeserializeHourTokensInTimeGroup(benchmark::State& state) {
  std::string hour_token_in_time_group =
      "0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1";
  std::vector<TokenCount> hour_tokens;

  for (auto _ : state) {
    auto result = Serialization::DeserializeHourTokensInTimeGroup(
        hour_token_in_time_group, hour_tokens);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_DeserializeHourTokensInTimeGroup);

}  // namespace
}  // namespace google::scp::pbs::budget_key_timeframe_manager

// Run the benchmark.
BENCHMARK_MAIN();
