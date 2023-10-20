/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "core/common/time_provider/src/stopwatch.h"

#include <gtest/gtest.h>

#include <thread>

using google::scp::core::common::Stopwatch;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::this_thread::sleep_for;

namespace google::scp::core::common::test {
TEST(StopwatchTest, TimeShouldElapse) {
  Stopwatch sw;
  sw.Start();
  sleep_for(milliseconds(120));
  auto elapsed = sw.Stop();

  EXPECT_GE(duration_cast<milliseconds>(elapsed).count(),
            milliseconds(120).count());
  // It could take longer than the 120 depending on how busy the machine is.
  // But we just want to make sure that it's within the expected range.
  EXPECT_LT(duration_cast<milliseconds>(elapsed).count(),
            milliseconds(150).count());
}

TEST(StopwatchTest, ShouldBeAbleToReuseStopWatch) {
  Stopwatch sw;
  sw.Start();
  sleep_for(milliseconds(120));
  auto elapsed = sw.Stop();

  EXPECT_GE(duration_cast<milliseconds>(elapsed).count(),
            milliseconds(120).count());
  // It could take longer than the 120 depending on how busy the machine is.
  // But we just want to make sure that it's within the expected range.
  EXPECT_LT(duration_cast<milliseconds>(elapsed).count(),
            milliseconds(150).count());

  // Start and stop again
  sw.Start();
  sleep_for(milliseconds(120));
  elapsed = sw.Stop();

  EXPECT_GE(duration_cast<milliseconds>(elapsed).count(),
            milliseconds(120).count());
  // It could take longer than the 120 depending on how busy the machine is.
  // But we just want to make sure that it's within the expected range.
  EXPECT_LT(duration_cast<milliseconds>(elapsed).count(),
            milliseconds(150).count());
}
}  // namespace google::scp::core::common::test
