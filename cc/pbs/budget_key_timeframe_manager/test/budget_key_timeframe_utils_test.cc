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

#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <list>
#include <utility>
#include <variant>
#include <vector>

#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/serialization/src/serialization.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_manager.h"
#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_serialization.h"
#include "pbs/budget_key_timeframe_manager/src/error_codes.h"
#include "pbs/budget_key_timeframe_manager/src/proto/budget_key_timeframe_manager.pb.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::common::Uuid;
using google::scp::pbs::BudgetKeyTimeframeManager;
using google::scp::pbs::budget_key_timeframe_manager::Utils;
using std::make_pair;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

using days = std::chrono::duration<int64_t, std::ratio<86400>>;

namespace google::scp::pbs::test {
TEST(BudgetKeyTimeframeManagerTest, GetTimeBucket) {
  uint64_t minute_increment_in_nano_seconds =
      60000000000;  // 1 minute increment
  for (uint64_t second = 1630498765350482296; second < 1660498765350482296;
       second += minute_increment_in_nano_seconds) {
    auto timestamp = second;
    auto date_time = std::chrono::nanoseconds(timestamp);
    std::chrono::system_clock::time_point converted_time_bucket(date_time);

    auto days_since_epoch = std::chrono::duration_cast<days>(
                                converted_time_bucket.time_since_epoch())
                                .count();

    auto seconds_since_epoch = timestamp / 1000000000;
    auto hours_since_epoch = seconds_since_epoch / 3600;
    auto calculated_days_since_epoch = seconds_since_epoch / 86400;

    EXPECT_EQ(calculated_days_since_epoch, days_since_epoch);
    auto calculated_hours_since_epoch = calculated_days_since_epoch * 24;
    auto bucket = hours_since_epoch - calculated_hours_since_epoch;
    EXPECT_GE(bucket, 0);
    EXPECT_LE(bucket, 23);
    EXPECT_EQ(bucket, Utils::GetTimeBucket(timestamp));
  }
}

TEST(BudgetKeyTimeframeManagerTest, GetTimeGroup) {
  uint64_t minute_increment_in_nano_seconds =
      60000000000;  // 1 minute increment
  for (uint64_t second = 1630498765350482296; second < 1660498765350482296;
       second += minute_increment_in_nano_seconds) {
    auto timestamp = second;
    auto date_time = std::chrono::nanoseconds(timestamp);
    std::chrono::system_clock::time_point converted_time_bucket(date_time);

    auto seconds_since_epoch = timestamp / 1000000000;
    auto calculated_days_since_epoch = seconds_since_epoch / 86400;

    EXPECT_EQ(calculated_days_since_epoch, Utils::GetTimeGroup(timestamp));
  }
}

TEST(BudgetKeyTimeframeManagerTest, GetUniqueTimeBuckets) {
  uint64_t minute_increment_in_nano_seconds =
      60000000000;  // 1 minute increment

  // 0 time buckets.
  EXPECT_EQ(Utils::GetUniqueTimeBuckets({}).size(), 0);

  // 1 time bucket.
  EXPECT_EQ(Utils::GetUniqueTimeBuckets({1}).size(), 1);

  // 1 time bucket.
  EXPECT_EQ(Utils::GetUniqueTimeBuckets({1, 1, 1, 1, 1}).size(), 1);

  // 1 time bucket, multiple entries
  EXPECT_EQ(Utils::GetUniqueTimeBuckets(
                {1, 1 + minute_increment_in_nano_seconds * 59})
                .size(),
            1);

  // 2 time buckets
  EXPECT_EQ(
      Utils::GetUniqueTimeBuckets({1, 1 + minute_increment_in_nano_seconds * 59,
                                   1 + minute_increment_in_nano_seconds * 60})
          .size(),
      2);

  // 2 time buckets
  EXPECT_EQ(
      Utils::GetUniqueTimeBuckets({1, 1 + minute_increment_in_nano_seconds * 59,
                                   1 + minute_increment_in_nano_seconds * 60,
                                   1 + minute_increment_in_nano_seconds * 59,
                                   1 + minute_increment_in_nano_seconds * 60})
          .size(),
      2);

  // 3 time buckets
  EXPECT_EQ(
      Utils::GetUniqueTimeBuckets({1, 1 + minute_increment_in_nano_seconds * 59,
                                   1 + minute_increment_in_nano_seconds * 60,
                                   1 + minute_increment_in_nano_seconds * 59,
                                   1 + minute_increment_in_nano_seconds * 60,
                                   1 + minute_increment_in_nano_seconds * 123})
          .size(),
      3);
}

TEST(BudgetKeyTimeframeManagerTest, GetUniqueTimeGroups) {
  uint64_t minute_increment_in_nano_seconds =
      60000000000;  // 1 minute increment

  // 0 time groups.
  EXPECT_EQ(Utils::GetUniqueTimeGroups({}).size(), 0);

  vector<core::Timestamp> timestamps = {1};

  // One time group
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 1);

  timestamps = {1, 1};

  // One time group
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 1);
  EXPECT_EQ(*Utils::GetUniqueTimeGroups(timestamps).begin(), 0);

  // Within a day, one time group
  timestamps = {1, 1 + minute_increment_in_nano_seconds * 60 * 23};
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 1);

  // Two days, two time groups
  timestamps = {1, 1 + minute_increment_in_nano_seconds * 60 * 24};
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 2);

  // Two days, two time groups
  timestamps = {1, 1 + minute_increment_in_nano_seconds * 60 * 24,
                1 + minute_increment_in_nano_seconds * 60 * 25};
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 2);

  // Two days, two time groups
  timestamps = {1, 1 + minute_increment_in_nano_seconds * 60 * 24,
                1 + minute_increment_in_nano_seconds * 60 * 25,
                1 + minute_increment_in_nano_seconds * 60 * 26};
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 2);

  // Two days, two time groups
  timestamps = {1, 1 + minute_increment_in_nano_seconds * 60 * 24,
                1 + minute_increment_in_nano_seconds * 60 * 25,
                1 + minute_increment_in_nano_seconds * 60 * 26,
                1 + minute_increment_in_nano_seconds * 60 * 47};
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 2);

  // Three days, three time groups
  timestamps = {1, 1 + minute_increment_in_nano_seconds * 60 * 24,
                1 + minute_increment_in_nano_seconds * 60 * 25,
                1 + minute_increment_in_nano_seconds * 60 * 26,
                0 + minute_increment_in_nano_seconds * 60 * 48};
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 3);

  // Three days, three time groups
  timestamps = {1, 1 + minute_increment_in_nano_seconds * 60 * 24,
                1 + minute_increment_in_nano_seconds * 60 * 25,
                1 + minute_increment_in_nano_seconds * 60 * 26,
                1 + minute_increment_in_nano_seconds * 60 * 48};
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 3);

  // Three days, three time groups, duplicates
  timestamps = {1,
                1 + minute_increment_in_nano_seconds * 60 * 24,
                1 + minute_increment_in_nano_seconds * 60 * 25,
                1 + minute_increment_in_nano_seconds * 60 * 26,
                1 + minute_increment_in_nano_seconds * 60 * 48,
                1 + minute_increment_in_nano_seconds * 60 * 24,
                1 + minute_increment_in_nano_seconds * 60 * 25,
                1 + minute_increment_in_nano_seconds * 60 * 26,
                1 + minute_increment_in_nano_seconds * 60 * 48};
  EXPECT_EQ(Utils::GetUniqueTimeGroups(timestamps).size(), 3);
}

}  // namespace google::scp::pbs::test
