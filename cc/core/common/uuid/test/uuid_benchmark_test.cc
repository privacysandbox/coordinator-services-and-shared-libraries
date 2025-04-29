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

#include <benchmark/benchmark.h>

#include "cc/core/common/uuid/src/uuid.h"

namespace privacy_sandbox::pbs_common {
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
}  // namespace privacy_sandbox::pbs_common

// Run the benchmark.
BENCHMARK_MAIN();
