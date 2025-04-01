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

#include <benchmark/benchmark.h>

#include "cc/core/interface/http_types.h"
#include "cc/core/utils/src/http.h"

namespace google::scp::core::utils {
namespace {
using ::privacy_sandbox::pbs_common::HttpHeaders;

void BM_ExtractValidUserAgent(benchmark::State& state) {
  HttpHeaders request_headers;
  request_headers.insert({"user-agent", "aggregation-service/2.5.0"});

  for (auto _ : state) {
    auto user_agent = ExtractUserAgent(request_headers);
    benchmark::DoNotOptimize(user_agent);
  }
}

void BM_ExtractInvalidUserAgent(benchmark::State& state) {
  HttpHeaders request_headers;
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
