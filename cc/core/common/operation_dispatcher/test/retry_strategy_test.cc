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

#include "core/common/operation_dispatcher/src/retry_strategy.h"

#include <gtest/gtest.h>

#include "core/interface/type_def.h"

namespace google::scp::core::common::test {
TEST(RetryStrategyTests, ExponentialRetryStrategyTest) {
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 1000, 5);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(0), 0);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(1), 1000);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(2), 2000);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(3), 4000);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(4), 8000);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(5), 16000);
  EXPECT_EQ(retry_strategy.GetMaximumAllowedRetryCount(), 5);
}

TEST(RetryStrategyTests, LinearRetryStrategyTest) {
  RetryStrategy retry_strategy(RetryStrategyType::Linear, 1000, 5);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(0), 0);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(1), 1000);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(2), 2000);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(3), 3000);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(4), 4000);
  EXPECT_EQ(retry_strategy.GetBackOffDurationInMilliseconds(5), 5000);
  EXPECT_EQ(retry_strategy.GetMaximumAllowedRetryCount(), 5);
}

}  // namespace google::scp::core::common::test
