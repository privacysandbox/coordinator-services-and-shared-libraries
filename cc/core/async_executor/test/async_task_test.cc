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

#include "cc/core/async_executor/src/async_task.h"

#include <gtest/gtest.h>

#include "cc/core/common/time_provider/src/time_provider.h"

using google::scp::core::common::TimeProvider;

namespace privacy_sandbox::pbs_common {
TEST(AsyncTaskTests, BasicTests) {
  auto func = []() {};
  auto current_time =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();

  AsyncTask async_task(func);
  EXPECT_GE(async_task.GetExecutionTimestamp(), current_time);
  AsyncTask async_task1(func, 1234);
  EXPECT_EQ(async_task1.GetExecutionTimestamp(), 1234);
}
}  // namespace privacy_sandbox::pbs_common
