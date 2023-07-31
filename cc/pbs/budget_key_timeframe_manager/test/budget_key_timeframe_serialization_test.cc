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

#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_serialization.h"

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
#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"
#include "pbs/budget_key_timeframe_manager/src/error_codes.h"
#include "pbs/budget_key_timeframe_manager/src/proto/budget_key_timeframe_manager.pb.h"

using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::common::Uuid;
using google::scp::pbs::BudgetKeyTimeframeManager;
using google::scp::pbs::budget_key_timeframe_manager::Serialization;
using google::scp::pbs::budget_key_timeframe_manager::Utils;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeGroupLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeManagerLog;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeManagerLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::OperationType;
using std::make_pair;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::pbs::test {
TEST(BudgetKeyTimeframeManagerTest, SerializeBudgetKeyTimeframeManagerLog) {
  BytesBuffer output_log;
  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  string log_body = "123456";
  budget_key_timeframe_manager_log.set_log_body(log_body.c_str(),
                                                log_body.length());

  EXPECT_EQ(Serialization::SerializeBudgetKeyTimeframeManagerLog(
                budget_key_timeframe_manager_log, output_log),
            SuccessExecutionResult());

  BudgetKeyTimeframeManagerLog deserialized_budget_key_timeframe_manager_log;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeManagerLog(
                output_log, deserialized_budget_key_timeframe_manager_log),
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_VERSION_IS_INVALID));

  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);

  EXPECT_EQ(Serialization::SerializeBudgetKeyTimeframeManagerLog(
                budget_key_timeframe_manager_log, output_log),
            SuccessExecutionResult());

  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeManagerLog(
                output_log, deserialized_budget_key_timeframe_manager_log),
            SuccessExecutionResult());

  EXPECT_EQ(deserialized_budget_key_timeframe_manager_log.version().major(), 1);
  EXPECT_EQ(deserialized_budget_key_timeframe_manager_log.version().minor(), 0);
  EXPECT_EQ(deserialized_budget_key_timeframe_manager_log.log_body(), "123456");
}

TEST(BudgetKeyTimeframeManagerTest, SerializeBudgetKeyTimeframeManagerLog_1_0) {
  BytesBuffer output_log;
  TimeGroup time_group = 1234;
  OperationType operation_type = OperationType::INSERT_TIMEGROUP_INTO_CACHE;

  BudgetKeyTimeframeManagerLog_1_0 budget_key_timeframe_manager_1_0_log;
  budget_key_timeframe_manager_1_0_log.set_operation_type(operation_type);
  budget_key_timeframe_manager_1_0_log.set_time_group(time_group);
  string log_body = "123456";
  budget_key_timeframe_manager_1_0_log.set_log_body(log_body.c_str(),
                                                    log_body.length());

  EXPECT_EQ(Serialization::SerializeBudgetKeyTimeframeManagerLog_1_0(
                budget_key_timeframe_manager_1_0_log, output_log),
            SuccessExecutionResult());

  string output_log_str(output_log.bytes->begin(), output_log.bytes->end());
  BudgetKeyTimeframeManagerLog_1_0
      deserialized_budget_key_timeframe_manager_1_0_log;
  EXPECT_EQ(
      Serialization::DeserializeBudgetKeyTimeframeManagerLog_1_0(
          output_log_str, deserialized_budget_key_timeframe_manager_1_0_log),
      SuccessExecutionResult());

  EXPECT_EQ(deserialized_budget_key_timeframe_manager_1_0_log.time_group(),
            1234);
  EXPECT_EQ(deserialized_budget_key_timeframe_manager_1_0_log.operation_type(),
            operation_type);
  EXPECT_EQ(deserialized_budget_key_timeframe_manager_1_0_log.log_body(),
            "123456");
}

TEST(BudgetKeyTimeframeManagerTest, SerializeBudgetKeyTimeframeLog_1_0) {
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(1234);
  budget_key_timeframe->active_token_count = 24;
  budget_key_timeframe->token_count = 13;
  budget_key_timeframe->active_transaction_id = Uuid::GenerateUuid();

  BytesBuffer output_log;
  EXPECT_EQ(Serialization::SerializeBudgetKeyTimeframeLog_1_0(
                budget_key_timeframe, output_log),
            SuccessExecutionResult());

  shared_ptr<BudgetKeyTimeframe> new_timeframe;
  string output_log_str(output_log.bytes->begin(), output_log.bytes->end());
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeLog_1_0(output_log_str,
                                                                new_timeframe),
            SuccessExecutionResult());

  EXPECT_EQ(new_timeframe->time_bucket_index,
            budget_key_timeframe->time_bucket_index);
  EXPECT_EQ(new_timeframe->active_transaction_id.load(),
            budget_key_timeframe->active_transaction_id.load());
  EXPECT_EQ(new_timeframe->token_count.load(),
            budget_key_timeframe->token_count.load());
  EXPECT_EQ(new_timeframe->active_token_count.load(),
            budget_key_timeframe->active_token_count.load());
}

TEST(BudgetKeyTimeframeManagerTest, SerializeBatchBudgetKeyTimeframeLog_1_0) {
  vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes;

  BytesBuffer output_log;
  EXPECT_EQ(Serialization::SerializeBatchBudgetKeyTimeframeLog_1_0(
                budget_key_timeframes, output_log),
            FailureExecutionResult(
                core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_INVALID_LOG));

  budget_key_timeframes.push_back(make_shared<BudgetKeyTimeframe>(1234));
  budget_key_timeframes.back()->active_token_count = 24;
  budget_key_timeframes.back()->token_count = 13;
  budget_key_timeframes.back()->active_transaction_id = Uuid::GenerateUuid();

  budget_key_timeframes.push_back(make_shared<BudgetKeyTimeframe>(12345));
  budget_key_timeframes.back()->active_token_count = 48;
  budget_key_timeframes.back()->token_count = 26;
  budget_key_timeframes.back()->active_transaction_id = Uuid::GenerateUuid();

  budget_key_timeframes.push_back(make_shared<BudgetKeyTimeframe>(123456));
  budget_key_timeframes.back()->active_token_count = 72;
  budget_key_timeframes.back()->token_count = 39;
  budget_key_timeframes.back()->active_transaction_id = Uuid::GenerateUuid();

  EXPECT_EQ(Serialization::SerializeBatchBudgetKeyTimeframeLog_1_0(
                budget_key_timeframes, output_log),
            SuccessExecutionResult());

  vector<shared_ptr<BudgetKeyTimeframe>> new_timeframes;
  string output_log_str(output_log.bytes->begin(), output_log.bytes->end());
  EXPECT_EQ(Serialization::DeserializeBatchBudgetKeyTimeframeLog_1_0(
                output_log_str, new_timeframes),
            SuccessExecutionResult());
  EXPECT_EQ(new_timeframes.size(), budget_key_timeframes.size());
  EXPECT_EQ(budget_key_timeframes.size(), 3);

  EXPECT_EQ(new_timeframes[0]->time_bucket_index, 1234);
  EXPECT_EQ(new_timeframes[0]->active_transaction_id.load(),
            budget_key_timeframes[0]->active_transaction_id.load());
  EXPECT_EQ(new_timeframes[0]->token_count.load(), 13);
  EXPECT_EQ(new_timeframes[0]->active_token_count.load(), 24);

  EXPECT_EQ(new_timeframes[1]->time_bucket_index, 12345);
  EXPECT_EQ(new_timeframes[1]->active_transaction_id.load(),
            budget_key_timeframes[1]->active_transaction_id.load());
  EXPECT_EQ(new_timeframes[1]->token_count.load(), 26);
  EXPECT_EQ(new_timeframes[1]->active_token_count.load(), 48);

  EXPECT_EQ(new_timeframes[2]->time_bucket_index, 123456);
  EXPECT_EQ(new_timeframes[2]->active_transaction_id.load(),
            budget_key_timeframes[2]->active_transaction_id.load());
  EXPECT_EQ(new_timeframes[2]->token_count.load(), 39);
  EXPECT_EQ(new_timeframes[2]->active_token_count.load(), 72);
}

TEST(BudgetKeyTimeframeManagerTest,
     DeserializeEmptyBatchBudgetKeyTimeframeLog_1_0) {
  vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes;
  string output_log_str = "";
  EXPECT_EQ(Serialization::DeserializeBatchBudgetKeyTimeframeLog_1_0(
                output_log_str, budget_key_timeframes),
            FailureExecutionResult(
                core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_INVALID_LOG));
  EXPECT_EQ(budget_key_timeframes.size(), 0);
}

TEST(BudgetKeyTimeframeManagerTest, SerializeBudgetKeyTimeframeGroupLog_1_0) {
  TimeGroup time_group = 1234;
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(1234);
  budget_key_timeframe->active_token_count = 24;
  budget_key_timeframe->token_count = 13;
  budget_key_timeframe->active_transaction_id = Uuid::GenerateUuid();

  auto budget_key_timeframe_2 = make_shared<BudgetKeyTimeframe>(345);
  budget_key_timeframe_2->active_token_count = 12;
  budget_key_timeframe_2->token_count = 3;
  budget_key_timeframe_2->active_transaction_id = Uuid::GenerateUuid();

  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto pair = make_pair(1234, budget_key_timeframe);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      pair, budget_key_timeframe);

  auto pair_2 = make_pair(345, budget_key_timeframe_2);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      pair, budget_key_timeframe_2);

  BytesBuffer output_log;
  EXPECT_EQ(Serialization::SerializeBudgetKeyTimeframeGroupLog_1_0(
                budget_key_timeframe_group, output_log),
            SuccessExecutionResult());
  string output_log_str(output_log.bytes->begin(), output_log.bytes->end());

  shared_ptr<BudgetKeyTimeframeGroup> new_timeframe_group;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeGroupLog_1_0(
                output_log_str, new_timeframe_group),
            SuccessExecutionResult());

  EXPECT_EQ(new_timeframe_group->time_group,
            budget_key_timeframe_group->time_group);

  vector<TimeGroup> old_keys;
  vector<TimeGroup> new_keys;

  budget_key_timeframe_group->budget_key_timeframes.Keys(old_keys);
  new_timeframe_group->budget_key_timeframes.Keys(new_keys);

  EXPECT_EQ(old_keys.size(), new_keys.size());
  for (size_t i = 0; i < old_keys.size(); ++i) {
    shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
    shared_ptr<BudgetKeyTimeframe> new_budget_key_timeframe;

    EXPECT_EQ(budget_key_timeframe_group->budget_key_timeframes.Find(
                  old_keys[i], budget_key_timeframe),
              SuccessExecutionResult());

    EXPECT_EQ(new_timeframe_group->budget_key_timeframes.Find(
                  old_keys[i], new_budget_key_timeframe),
              SuccessExecutionResult());

    EXPECT_EQ(new_budget_key_timeframe->active_transaction_id.load(),
              budget_key_timeframe->active_transaction_id.load());
    EXPECT_EQ(new_budget_key_timeframe->token_count.load(),
              budget_key_timeframe->token_count.load());
    EXPECT_EQ(new_budget_key_timeframe->active_token_count.load(),
              budget_key_timeframe->active_token_count.load());
  }
}

TEST(BudgetKeyTimeframeManagerTest, SerializeBudgetKeyTimeframeLog) {
  TimeGroup time_group = 1234;
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(1234);
  budget_key_timeframe->active_token_count = 24;
  budget_key_timeframe->token_count = 13;
  budget_key_timeframe->active_transaction_id = Uuid::GenerateUuid();

  BytesBuffer output_log;
  EXPECT_EQ(Serialization::SerializeBudgetKeyTimeframeLog(
                time_group, budget_key_timeframe, output_log),
            SuccessExecutionResult());

  BudgetKeyTimeframeManagerLog budget_key_time_frame_manager_log;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeManagerLog(
                output_log, budget_key_time_frame_manager_log),
            SuccessExecutionResult());

  BudgetKeyTimeframeManagerLog_1_0 budget_key_time_frame_manager_log_1_0;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeManagerLog_1_0(
                budget_key_time_frame_manager_log.log_body(),
                budget_key_time_frame_manager_log_1_0),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_time_frame_manager_log_1_0.operation_type(),
            OperationType::UPDATE_TIMEFRAME_RECORD);

  shared_ptr<BudgetKeyTimeframe> new_budget_key_timeframe;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeLog_1_0(
                budget_key_time_frame_manager_log_1_0.log_body(),
                new_budget_key_timeframe),
            SuccessExecutionResult());

  EXPECT_EQ(new_budget_key_timeframe->time_bucket_index,
            budget_key_timeframe->time_bucket_index);
  EXPECT_EQ(new_budget_key_timeframe->active_transaction_id.load(),
            budget_key_timeframe->active_transaction_id.load());
  EXPECT_EQ(new_budget_key_timeframe->token_count.load(),
            budget_key_timeframe->token_count.load());
  EXPECT_EQ(new_budget_key_timeframe->active_token_count.load(),
            budget_key_timeframe->active_token_count.load());
}

TEST(BudgetKeyTimeframeManagerTest, SerializeBatchBudgetKeyTimeframeLog) {
  TimeGroup time_group = 1234;
  vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes;
  budget_key_timeframes.push_back(make_shared<BudgetKeyTimeframe>(1234));
  budget_key_timeframes.back()->active_token_count = 24;
  budget_key_timeframes.back()->token_count = 13;
  budget_key_timeframes.back()->active_transaction_id = Uuid::GenerateUuid();

  budget_key_timeframes.push_back(make_shared<BudgetKeyTimeframe>(12345));
  budget_key_timeframes.back()->active_token_count = 48;
  budget_key_timeframes.back()->token_count = 26;
  budget_key_timeframes.back()->active_transaction_id = Uuid::GenerateUuid();

  budget_key_timeframes.push_back(make_shared<BudgetKeyTimeframe>(123456));
  budget_key_timeframes.back()->active_token_count = 72;
  budget_key_timeframes.back()->token_count = 39;
  budget_key_timeframes.back()->active_transaction_id = Uuid::GenerateUuid();

  BytesBuffer output_log;
  EXPECT_EQ(Serialization::SerializeBatchBudgetKeyTimeframeLog(
                time_group, budget_key_timeframes, output_log),
            SuccessExecutionResult());

  BudgetKeyTimeframeManagerLog budget_key_time_frame_manager_log;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeManagerLog(
                output_log, budget_key_time_frame_manager_log),
            SuccessExecutionResult());

  BudgetKeyTimeframeManagerLog_1_0 budget_key_time_frame_manager_log_1_0;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeManagerLog_1_0(
                budget_key_time_frame_manager_log.log_body(),
                budget_key_time_frame_manager_log_1_0),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_time_frame_manager_log_1_0.time_group(), time_group);
  EXPECT_EQ(budget_key_time_frame_manager_log_1_0.operation_type(),
            OperationType::BATCH_UPDATE_TIMEFRAME_RECORDS_OF_TIMEGROUP);

  vector<shared_ptr<BudgetKeyTimeframe>> new_timeframes;
  EXPECT_EQ(
      Serialization::DeserializeBatchBudgetKeyTimeframeLog_1_0(
          budget_key_time_frame_manager_log_1_0.log_body(), new_timeframes),
      SuccessExecutionResult());

  EXPECT_EQ(new_timeframes[0]->time_bucket_index, 1234);
  EXPECT_EQ(new_timeframes[0]->active_transaction_id.load(),
            budget_key_timeframes[0]->active_transaction_id.load());
  EXPECT_EQ(new_timeframes[0]->token_count.load(), 13);
  EXPECT_EQ(new_timeframes[0]->active_token_count.load(), 24);

  EXPECT_EQ(new_timeframes[1]->time_bucket_index, 12345);
  EXPECT_EQ(new_timeframes[1]->active_transaction_id.load(),
            budget_key_timeframes[1]->active_transaction_id.load());
  EXPECT_EQ(new_timeframes[1]->token_count.load(), 26);
  EXPECT_EQ(new_timeframes[1]->active_token_count.load(), 48);

  EXPECT_EQ(new_timeframes[2]->time_bucket_index, 123456);
  EXPECT_EQ(new_timeframes[2]->active_transaction_id.load(),
            budget_key_timeframes[2]->active_transaction_id.load());
  EXPECT_EQ(new_timeframes[2]->token_count.load(), 39);
  EXPECT_EQ(new_timeframes[2]->active_token_count.load(), 72);
}

TEST(BudgetKeyTimeframeManagerTest, SerializeBudgetKeyTimeframeGroupLog) {
  TimeGroup time_group = 1234;
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(1234);
  budget_key_timeframe->active_token_count = 24;
  budget_key_timeframe->token_count = 13;
  budget_key_timeframe->active_transaction_id = Uuid::GenerateUuid();

  auto budget_key_timeframe_2 = make_shared<BudgetKeyTimeframe>(345);
  budget_key_timeframe_2->active_token_count = 12;
  budget_key_timeframe_2->token_count = 3;
  budget_key_timeframe_2->active_transaction_id = Uuid::GenerateUuid();

  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto pair = make_pair(1234, budget_key_timeframe);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      pair, budget_key_timeframe);

  auto pair_2 = make_pair(345, budget_key_timeframe_2);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      pair, budget_key_timeframe_2);

  BytesBuffer output_log;
  EXPECT_EQ(Serialization::SerializeBudgetKeyTimeframeGroupLog(
                budget_key_timeframe_group, output_log),
            SuccessExecutionResult());

  BudgetKeyTimeframeManagerLog budget_key_time_frame_manager_log;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeManagerLog(
                output_log, budget_key_time_frame_manager_log),
            SuccessExecutionResult());

  BudgetKeyTimeframeManagerLog_1_0 budget_key_time_frame_manager_log_1_0;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeManagerLog_1_0(
                budget_key_time_frame_manager_log.log_body(),
                budget_key_time_frame_manager_log_1_0),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_time_frame_manager_log_1_0.operation_type(),
            OperationType::INSERT_TIMEGROUP_INTO_CACHE);

  shared_ptr<BudgetKeyTimeframeGroup> new_timeframe_group;
  EXPECT_EQ(Serialization::DeserializeBudgetKeyTimeframeGroupLog_1_0(
                budget_key_time_frame_manager_log_1_0.log_body(),
                new_timeframe_group),
            SuccessExecutionResult());

  EXPECT_EQ(new_timeframe_group->time_group,
            budget_key_timeframe_group->time_group);

  vector<TimeGroup> old_keys;
  vector<TimeGroup> new_keys;

  budget_key_timeframe_group->budget_key_timeframes.Keys(old_keys);
  new_timeframe_group->budget_key_timeframes.Keys(new_keys);

  EXPECT_EQ(old_keys.size(), new_keys.size());
  for (size_t i = 0; i < old_keys.size(); ++i) {
    shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
    shared_ptr<BudgetKeyTimeframe> new_budget_key_timeframe;

    EXPECT_EQ(budget_key_timeframe_group->budget_key_timeframes.Find(
                  old_keys[i], budget_key_timeframe),
              SuccessExecutionResult());

    EXPECT_EQ(new_timeframe_group->budget_key_timeframes.Find(
                  old_keys[i], new_budget_key_timeframe),
              SuccessExecutionResult());

    EXPECT_EQ(new_budget_key_timeframe->active_transaction_id.load(),
              budget_key_timeframe->active_transaction_id.load());
    EXPECT_EQ(new_budget_key_timeframe->token_count.load(),
              budget_key_timeframe->token_count.load());
    EXPECT_EQ(new_budget_key_timeframe->active_token_count.load(),
              budget_key_timeframe->active_token_count.load());
  }
}

TEST(BudgetKeyTimeframeManagerTest, SerializeHourTokensInTimeGroup) {
  for (int i = 0; i < 240; i++) {
    vector<TokenCount> tokens(i, 1);
    string hour_token_in_time_group;
    if (i != 24) {
      EXPECT_EQ(
          Serialization::SerializeHourTokensInTimeGroup(
              tokens, hour_token_in_time_group),
          FailureExecutionResult(
              core::errors::
                  SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA));
    } else {
      EXPECT_EQ(Serialization::SerializeHourTokensInTimeGroup(
                    tokens, hour_token_in_time_group),
                SuccessExecutionResult());

      EXPECT_EQ(hour_token_in_time_group,
                "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1");
    }
  }
}

TEST(BudgetKeyTimeframeManagerTest, DeserializeHourTokensInTimeGroup) {
  for (int i = 0; i < 240; i++) {
    vector<TokenCount> tokens;
    string hour_token_in_time_group;
    for (int j = 0; j < i; ++j) {
      hour_token_in_time_group += "1";
      if (j < i - 1) {
        hour_token_in_time_group += " ";
      }
    }

    if (i != 24) {
      EXPECT_EQ(
          Serialization::DeserializeHourTokensInTimeGroup(
              hour_token_in_time_group, tokens),
          FailureExecutionResult(
              core::errors::
                  SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA));
    } else {
      EXPECT_EQ(Serialization::DeserializeHourTokensInTimeGroup(
                    hour_token_in_time_group, tokens),
                SuccessExecutionResult());
      for (int k = 0; k < 24; ++k) {
        EXPECT_EQ(tokens[k], 1);
      }
    }
  }

  vector<TokenCount> tokens;
  string hour_token_in_time_group =
      "a a a a a a a a a a a a a a a a a a a a a a a a";
  EXPECT_EQ(Serialization::DeserializeHourTokensInTimeGroup(
                hour_token_in_time_group, tokens),
            FailureExecutionResult(
                core::errors::
                    SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA));
}

}  // namespace google::scp::pbs::test
