// Copyright 2022 Google LLC
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

#include "conditional_wait.h"

#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>

#include "core/common/time_provider/src/time_provider.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::common::TimeProvider;
using std::function;
using std::this_thread::yield;

namespace google::scp::core::test {
void WaitUntil(function<bool()> condition, DurationMs timeout) {
  auto start_time = TimeProvider::GetSteadyTimestampInNanoseconds();
  while (!condition()) {
    auto now_time = TimeProvider::GetSteadyTimestampInNanoseconds();
    auto duration = now_time - start_time;
    if (duration > timeout) {
      throw std::runtime_error("test timeout");
    }
    yield();
  }
}
}  // namespace google::scp::core::test
