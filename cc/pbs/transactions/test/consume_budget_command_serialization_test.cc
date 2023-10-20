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

#include "pbs/transactions/src/consume_budget_command_serialization.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::TransactionCommand;
using google::scp::core::common::Uuid;
using google::scp::pbs::ConsumeBudgetCommandSerialization;
using std::make_shared;
using std::move;
using std::nullopt;
using std::shared_ptr;
using std::static_pointer_cast;
using std::vector;

namespace google::scp::pbs::test {
TEST(ConsumeBudgetCommandSerializationTest,
     SerializeVersion_1_0_InvalidTransactionCommand) {
  Uuid transaction_id = Uuid::GenerateUuid();
  BytesBuffer bytes_buffer;
  auto transaction_command = make_shared<TransactionCommand>();

  EXPECT_EQ(
      ConsumeBudgetCommandSerialization::SerializeVersion_1_0(
          transaction_id, transaction_command, bytes_buffer),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE));

  bytes_buffer.length = 1;
  bytes_buffer.bytes = make_shared<vector<Byte>>(1);

  EXPECT_EQ(
      ConsumeBudgetCommandSerialization::SerializeVersion_1_0(
          transaction_id, transaction_command, bytes_buffer),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE));
}

TEST(ConsumeBudgetCommandSerializationTest,
     SerializeVersion_1_1_InvalidTransactionCommand) {
  Uuid transaction_id = Uuid::GenerateUuid();
  BytesBuffer bytes_buffer;
  auto transaction_command = make_shared<TransactionCommand>();

  EXPECT_EQ(
      ConsumeBudgetCommandSerialization::SerializeVersion_1_1(
          transaction_id, transaction_command, bytes_buffer),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE));

  bytes_buffer.length = 1;
  bytes_buffer.bytes = make_shared<vector<Byte>>(1);

  EXPECT_EQ(
      ConsumeBudgetCommandSerialization::SerializeVersion_1_1(
          transaction_id, transaction_command, bytes_buffer),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE));
}

TEST(ConsumeBudgetCommandSerializationTest, SerializeVersion_1_0) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");
  Timestamp time_bucket = 1000;
  uint8_t total_budget_to_consume = 10;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  shared_ptr<TransactionCommand> consume_budget_command =
      make_shared<ConsumeBudgetCommand>(
          transaction_id, budget_key_name,
          ConsumeBudgetCommandRequestInfo{time_bucket, total_budget_to_consume},
          async_executor, budget_key_provider);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(ConsumeBudgetCommandSerialization::SerializeVersion_1_0(
                transaction_id, consume_budget_command, bytes_buffer),
            SuccessExecutionResult());
}

TEST(ConsumeBudgetCommandSerializationTest, SerializeVersion_1_1) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");
  Timestamp time_bucket = 1000;
  uint8_t total_budget_to_consume = 10;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  shared_ptr<TransactionCommand> consume_budget_command =
      make_shared<ConsumeBudgetCommand>(
          transaction_id, budget_key_name,
          ConsumeBudgetCommandRequestInfo{time_bucket, total_budget_to_consume},
          async_executor, budget_key_provider);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(ConsumeBudgetCommandSerialization::SerializeVersion_1_1(
                transaction_id, consume_budget_command, bytes_buffer),
            SuccessExecutionResult());
}

TEST(ConsumeBudgetCommandSerializationTest, DeserializeVersion_1_0_Failure) {
  Uuid transaction_id = Uuid::GenerateUuid();
  shared_ptr<TransactionCommand> consume_budget_command;
  BytesBuffer bytes_buffer;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  EXPECT_EQ(
      ConsumeBudgetCommandSerialization::DeserializeVersion_1_0(
          transaction_id, bytes_buffer, async_executor, budget_key_provider,
          consume_budget_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED));

  bytes_buffer.bytes = make_shared<vector<Byte>>(1);
  bytes_buffer.length = 1;
  bytes_buffer.bytes->push_back(1);

  EXPECT_EQ(
      ConsumeBudgetCommandSerialization::DeserializeVersion_1_0(
          transaction_id, bytes_buffer, async_executor, budget_key_provider,
          consume_budget_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED));
}

TEST(ConsumeBudgetCommandSerializationTest, DeserializeVersion_1_1_Failure) {
  Uuid transaction_id = Uuid::GenerateUuid();
  shared_ptr<TransactionCommand> consume_budget_command;
  BytesBuffer bytes_buffer;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  EXPECT_EQ(
      ConsumeBudgetCommandSerialization::DeserializeVersion_1_1(
          transaction_id, bytes_buffer, async_executor, budget_key_provider,
          consume_budget_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED));

  bytes_buffer.bytes = make_shared<vector<Byte>>(1);
  bytes_buffer.length = 1;
  bytes_buffer.bytes->push_back(1);

  EXPECT_EQ(
      ConsumeBudgetCommandSerialization::DeserializeVersion_1_1(
          transaction_id, bytes_buffer, async_executor, budget_key_provider,
          consume_budget_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED));
}

TEST(ConsumeBudgetCommandSerializationTest, SerializeDeserializeVersion_1_0) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  ConsumeBudgetCommandRequestInfo budget_consumption(1, 2, 3);
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  shared_ptr<TransactionCommand> consume_budget_command =
      make_shared<ConsumeBudgetCommand>(transaction_id, budget_key_name,
                                        move(budget_consumption),
                                        async_executor, budget_key_provider);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(ConsumeBudgetCommandSerialization::SerializeVersion_1_0(
                transaction_id, consume_budget_command, bytes_buffer),
            SuccessExecutionResult());

  shared_ptr<TransactionCommand> deserialized_consume_budget_command;
  EXPECT_EQ(ConsumeBudgetCommandSerialization::DeserializeVersion_1_0(
                transaction_id, bytes_buffer, async_executor,
                budget_key_provider, deserialized_consume_budget_command),
            SuccessExecutionResult());

  auto old_consume_budget_command =
      static_pointer_cast<ConsumeBudgetCommand>(consume_budget_command);
  auto new_consume_budget_command = static_pointer_cast<ConsumeBudgetCommand>(
      deserialized_consume_budget_command);
  EXPECT_EQ(*new_consume_budget_command->GetBudgetKeyName(),
            *old_consume_budget_command->GetBudgetKeyName());
  EXPECT_EQ(new_consume_budget_command->GetTimeBucket(),
            old_consume_budget_command->GetTimeBucket());
  EXPECT_EQ(new_consume_budget_command->GetTokenCount(),
            old_consume_budget_command->GetTokenCount());
  EXPECT_EQ(new_consume_budget_command->GetVersion(),
            old_consume_budget_command->GetVersion());
  EXPECT_EQ(new_consume_budget_command->GetBudgetConsumption(),
            old_consume_budget_command->GetBudgetConsumption());

  // Request index even if present should not be present in the deserialized
  // version since the command version protocol does not allow it to be
  // serialized.
  EXPECT_TRUE(old_consume_budget_command->GetBudgetConsumption()
                  .request_index.has_value());
  EXPECT_FALSE(new_consume_budget_command->GetBudgetConsumption()
                   .request_index.has_value());
}

TEST(ConsumeBudgetCommandSerializationTest, SerializeDeserializeVersion_1_1) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  ConsumeBudgetCommandRequestInfo budget_consumption(1, 2, 3);
  shared_ptr<TransactionCommand> consume_budget_command =
      make_shared<ConsumeBudgetCommand>(transaction_id, budget_key_name,
                                        move(budget_consumption),
                                        async_executor, budget_key_provider);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(ConsumeBudgetCommandSerialization::SerializeVersion_1_1(
                transaction_id, consume_budget_command, bytes_buffer),
            SuccessExecutionResult());

  shared_ptr<TransactionCommand> deserialized_consume_budget_command;
  EXPECT_EQ(ConsumeBudgetCommandSerialization::DeserializeVersion_1_1(
                transaction_id, bytes_buffer, async_executor,
                budget_key_provider, deserialized_consume_budget_command),
            SuccessExecutionResult());

  auto old_consume_budget_command =
      static_pointer_cast<ConsumeBudgetCommand>(consume_budget_command);
  auto new_consume_budget_command = static_pointer_cast<ConsumeBudgetCommand>(
      deserialized_consume_budget_command);
  EXPECT_EQ(*new_consume_budget_command->GetBudgetKeyName(),
            *old_consume_budget_command->GetBudgetKeyName());
  EXPECT_EQ(new_consume_budget_command->GetVersion(),
            old_consume_budget_command->GetVersion());
  EXPECT_EQ(new_consume_budget_command->GetTimeBucket(),
            old_consume_budget_command->GetTimeBucket());
  EXPECT_EQ(new_consume_budget_command->GetTokenCount(),
            old_consume_budget_command->GetTokenCount());
  EXPECT_EQ(new_consume_budget_command->GetBudgetConsumption(),
            old_consume_budget_command->GetBudgetConsumption());

  // Ensure optional field is serialized/deserialized.
  EXPECT_TRUE(old_consume_budget_command->GetBudgetConsumption()
                  .request_index.has_value());
  EXPECT_TRUE(new_consume_budget_command->GetBudgetConsumption()
                  .request_index.has_value());
}

TEST(ConsumeBudgetCommandSerializationTest,
     SerializeDeserializeIgnoreOptionalFieldsVersion_1_1) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  ConsumeBudgetCommandRequestInfo budget_consumption(1, 2);

  // Do not set the optional field for serialization.
  budget_consumption.request_index = nullopt;

  shared_ptr<TransactionCommand> consume_budget_command =
      make_shared<ConsumeBudgetCommand>(transaction_id, budget_key_name,
                                        move(budget_consumption),
                                        async_executor, budget_key_provider);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(ConsumeBudgetCommandSerialization::SerializeVersion_1_1(
                transaction_id, consume_budget_command, bytes_buffer),
            SuccessExecutionResult());

  shared_ptr<TransactionCommand> deserialized_consume_budget_command;
  EXPECT_EQ(ConsumeBudgetCommandSerialization::DeserializeVersion_1_1(
                transaction_id, bytes_buffer, async_executor,
                budget_key_provider, deserialized_consume_budget_command),
            SuccessExecutionResult());

  auto old_consume_budget_command =
      static_pointer_cast<ConsumeBudgetCommand>(consume_budget_command);
  auto new_consume_budget_command = static_pointer_cast<ConsumeBudgetCommand>(
      deserialized_consume_budget_command);
  EXPECT_EQ(*new_consume_budget_command->GetBudgetKeyName(),
            *old_consume_budget_command->GetBudgetKeyName());
  EXPECT_EQ(new_consume_budget_command->GetVersion(),
            old_consume_budget_command->GetVersion());
  EXPECT_EQ(new_consume_budget_command->GetTimeBucket(),
            old_consume_budget_command->GetTimeBucket());
  EXPECT_EQ(new_consume_budget_command->GetTokenCount(),
            old_consume_budget_command->GetTokenCount());
  EXPECT_EQ(new_consume_budget_command->GetBudgetConsumption(),
            old_consume_budget_command->GetBudgetConsumption());

  // Ensure optional field is not serialized/deserialized.
  EXPECT_FALSE(old_consume_budget_command->GetBudgetConsumption()
                   .request_index.has_value());
  EXPECT_FALSE(new_consume_budget_command->GetBudgetConsumption()
                   .request_index.has_value());
}

}  // namespace google::scp::pbs::test
