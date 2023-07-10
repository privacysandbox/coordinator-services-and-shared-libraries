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

#include "pbs/transactions/src/batch_consume_budget_command_serialization.h"

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
using google::scp::pbs::BatchConsumeBudgetCommandSerialization;
using std::make_shared;
using std::shared_ptr;
using std::static_pointer_cast;
using std::vector;

namespace google::scp::pbs::test {
TEST(BatchConsumeBudgetCommandSerializationTest,
     SerializeVersion_1_0_InvalidTransactionCommand) {
  Uuid transaction_id = Uuid::GenerateUuid();
  BytesBuffer bytes_buffer;
  auto transaction_command = make_shared<TransactionCommand>();

  EXPECT_EQ(
      BatchConsumeBudgetCommandSerialization::SerializeVersion_1_0(
          transaction_id, transaction_command, bytes_buffer),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE));

  bytes_buffer.length = 1;
  bytes_buffer.bytes = make_shared<vector<Byte>>(1);

  EXPECT_EQ(
      BatchConsumeBudgetCommandSerialization::SerializeVersion_1_0(
          transaction_id, transaction_command, bytes_buffer),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE));
}

TEST(BatchConsumeBudgetCommandSerializationTest, SerializeVersion_1_0) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(100, 2, 0);
  budget_consumptions.emplace_back(200, 4, 1);

  shared_ptr<TransactionCommand> batch_consume_budget_command =
      make_shared<BatchConsumeBudgetCommand>(
          transaction_id, budget_key_name, move(budget_consumptions),
          async_executor, budget_key_provider);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(BatchConsumeBudgetCommandSerialization::SerializeVersion_1_0(
                transaction_id, batch_consume_budget_command, bytes_buffer),
            SuccessExecutionResult());
}

TEST(BatchConsumeBudgetCommandSerializationTest, DeserializeVersion_1_0) {
  Uuid transaction_id = Uuid::GenerateUuid();
  shared_ptr<TransactionCommand> batch_consume_budget_command;
  BytesBuffer bytes_buffer;
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  EXPECT_EQ(
      BatchConsumeBudgetCommandSerialization::DeserializeVersion_1_0(
          transaction_id, bytes_buffer, async_executor, budget_key_provider,
          batch_consume_budget_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED));

  bytes_buffer.bytes = make_shared<vector<Byte>>(1);
  bytes_buffer.length = 1;
  bytes_buffer.bytes->push_back(1);

  EXPECT_EQ(
      BatchConsumeBudgetCommandSerialization::DeserializeVersion_1_0(
          transaction_id, bytes_buffer, async_executor, budget_key_provider,
          batch_consume_budget_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED));
}

TEST(BatchConsumeBudgetCommandSerializationTest,
     SerializeDeserializeVersion_1_0) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(100, 2, 10);
  budget_consumptions.emplace_back(200, 4, 1);

  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  shared_ptr<TransactionCommand> batch_consume_budget_command =
      make_shared<BatchConsumeBudgetCommand>(
          transaction_id, budget_key_name, move(budget_consumptions),
          async_executor, budget_key_provider);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(BatchConsumeBudgetCommandSerialization::SerializeVersion_1_0(
                transaction_id, batch_consume_budget_command, bytes_buffer),
            SuccessExecutionResult());

  shared_ptr<TransactionCommand> deserialized_batch_consume_budget_command;
  EXPECT_EQ(BatchConsumeBudgetCommandSerialization::DeserializeVersion_1_0(
                transaction_id, bytes_buffer, async_executor,
                budget_key_provider, deserialized_batch_consume_budget_command),
            SuccessExecutionResult());

  auto old_batch_consume_budget_command =
      static_pointer_cast<BatchConsumeBudgetCommand>(
          batch_consume_budget_command);
  auto new_batch_consume_budget_command =
      static_pointer_cast<BatchConsumeBudgetCommand>(
          deserialized_batch_consume_budget_command);
  EXPECT_EQ(*new_batch_consume_budget_command->GetBudgetKeyName(),
            *old_batch_consume_budget_command->GetBudgetKeyName());
  EXPECT_EQ(new_batch_consume_budget_command->GetBudgetConsumptions().size(),
            2);
  EXPECT_EQ(old_batch_consume_budget_command->GetBudgetConsumptions().size(),
            2);
  EXPECT_EQ(new_batch_consume_budget_command->GetBudgetConsumptions(),
            old_batch_consume_budget_command->GetBudgetConsumptions());
  EXPECT_EQ(new_batch_consume_budget_command->GetVersion(),
            old_batch_consume_budget_command->GetVersion());
  EXPECT_TRUE(old_batch_consume_budget_command->GetBudgetConsumptions()[0]
                  .request_index.has_value());
  EXPECT_TRUE(old_batch_consume_budget_command->GetBudgetConsumptions()[1]
                  .request_index.has_value());
  EXPECT_TRUE(new_batch_consume_budget_command->GetBudgetConsumptions()[0]
                  .request_index.has_value());
  EXPECT_TRUE(new_batch_consume_budget_command->GetBudgetConsumptions()[1]
                  .request_index.has_value());
}
}  // namespace google::scp::pbs::test
