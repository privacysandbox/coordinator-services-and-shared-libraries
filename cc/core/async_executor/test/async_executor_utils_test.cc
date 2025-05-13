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

#include "cc/core/async_executor/src/async_executor_utils.h"

#include <gtest/gtest.h>

#include <thread>

#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {
namespace {

TEST(AsyncExecutorUtilsTest, BasicTests) {
  EXPECT_SUCCESS(AsyncExecutorUtils::SetAffinity(1));
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  EXPECT_NE(CPU_ISSET(1, &cpuset), 0);

  // Invalid CPU.
  EXPECT_THAT(
      AsyncExecutorUtils::SetAffinity(std::thread::hardware_concurrency()),
      ResultIs(
          FailureExecutionResult(SC_ASYNC_EXECUTOR_UNABLE_TO_SET_AFFINITY)));
}
}  // namespace
}  // namespace privacy_sandbox::pbs_common
