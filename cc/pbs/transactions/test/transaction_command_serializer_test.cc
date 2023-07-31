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

#include "pbs/transactions/src/transaction_command_serializer.h"

#include <gtest/gtest.h>

#include <memory>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/transactions/src/batch_consume_budget_command.h"
#include "pbs/transactions/src/consume_budget_command.h"
#include "pbs/transactions/src/error_codes.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::TransactionCommand;
using google::scp::core::common::Uuid;
using google::scp::pbs::BatchConsumeBudgetCommand;
using google::scp::pbs::ConsumeBudgetCommand;
using google::scp::pbs::TransactionCommandSerializer;
using google::scp::pbs::transactions::proto::CommandType;
using google::scp::pbs::transactions::proto::TransactionCommandLog;
using google::scp::pbs::transactions::proto::TransactionCommandLog_1_0;
using std::make_shared;
using std::move;
using std::pair;
using std::shared_ptr;
using std::static_pointer_cast;
using std::vector;

namespace google::scp::pbs::test {

class TransactionSerializerPrivateAccessor
    : public TransactionCommandSerializer {
 public:
  TransactionSerializerPrivateAccessor()
      : TransactionCommandSerializer(nullptr, nullptr) {}

  ExecutionResult CanSerialize(shared_ptr<TransactionCommand> command) {
    return TransactionCommandSerializer::CanSerialize(command);
  }

  ExecutionResult CanDeserialize(TransactionCommandLog& log) {
    return TransactionCommandSerializer::CanDeserialize(log);
  }
};

TEST(TransactionCommandSerializerTest, CanSerialize) {
  TransactionSerializerPrivateAccessor serializer;
  auto transaction_command = make_shared<TransactionCommand>();
  EXPECT_EQ(
      serializer.CanSerialize(transaction_command),
      FailureExecutionResult(
          core::errors::SC_PBS_TRANSACTION_COMMAND_SERIALIZER_UNSUPPORTED));
  transaction_command->command_id = kConsumeBudgetCommandId;
  EXPECT_EQ(serializer.CanSerialize(transaction_command),
            SuccessExecutionResult());
  transaction_command->command_id = kBatchConsumeBudgetCommandId;
  EXPECT_EQ(serializer.CanSerialize(transaction_command),
            SuccessExecutionResult());
  transaction_command->command_id = {0, 0};
  EXPECT_EQ(
      serializer.CanSerialize(transaction_command),
      FailureExecutionResult(
          core::errors::SC_PBS_TRANSACTION_COMMAND_SERIALIZER_UNSUPPORTED));
}

TEST(TransactionCommandSerializerTest, CanDeserialize) {
  TransactionSerializerPrivateAccessor serializer;
  TransactionCommandLog transaction_command_log;
  transaction_command_log.mutable_version()->set_major(1);
  transaction_command_log.mutable_version()->set_minor(0);
  EXPECT_EQ(serializer.CanDeserialize(transaction_command_log),
            SuccessExecutionResult());

  transaction_command_log.mutable_version()->set_major(1);
  transaction_command_log.mutable_version()->set_minor(2);

  auto error = core::errors::
      SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_VERSION;
  EXPECT_EQ(serializer.CanDeserialize(transaction_command_log),
            FailureExecutionResult(error));
}

TEST(TransactionCommandSerializerTest, InvalidTransactionCommand) {
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<TransactionCommand> transaction_command =
      make_shared<TransactionCommand>();

  TransactionCommandSerializer transaction_command_serializer(
      async_executor, budget_key_provider);

  Uuid transaction_id = Uuid::GenerateUuid();
  BytesBuffer bytes_buffer;
  EXPECT_EQ(
      transaction_command_serializer.Serialize(
          transaction_id, transaction_command, bytes_buffer),
      FailureExecutionResult(
          core::errors::SC_PBS_TRANSACTION_COMMAND_SERIALIZER_UNSUPPORTED));
}

TEST(TransactionCommandSerializerTest, InvalidBytesBuffer) {
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<TransactionCommand> transaction_command =
      make_shared<TransactionCommand>();

  TransactionCommandSerializer transaction_command_serializer(
      async_executor, budget_key_provider);

  Uuid transaction_id = Uuid::GenerateUuid();
  BytesBuffer bytes_buffer;
  EXPECT_EQ(
      transaction_command_serializer.Deserialize(transaction_id, bytes_buffer,
                                                 transaction_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_TRANSACTION_LOG));
}

TEST(TransactionCommandSerializerTest, InvalidTransactionLogVersion) {
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<TransactionCommand> transaction_command =
      make_shared<TransactionCommand>();

  TransactionCommandSerializer transaction_command_serializer(
      async_executor, budget_key_provider);

  TransactionCommandLog transaction_command_log;
  transaction_command_log.mutable_version()->set_major(1);
  transaction_command_log.mutable_version()->set_minor(2);

  Uuid transaction_id = Uuid::GenerateUuid();
  auto size = transaction_command_log.ByteSizeLong();
  BytesBuffer bytes_buffer(size);
  transaction_command_log.SerializeToArray(bytes_buffer.bytes->data(), size);
  bytes_buffer.length = size;

  EXPECT_EQ(
      transaction_command_serializer.Deserialize(transaction_id, bytes_buffer,
                                                 transaction_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_VERSION));
}

TEST(TransactionCommandSerializerTest, InvalidTransactionLogType) {
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<TransactionCommand> transaction_command =
      make_shared<TransactionCommand>();

  TransactionCommandSerializer transaction_command_serializer(
      async_executor, budget_key_provider);

  TransactionCommandLog transaction_command_log;
  transaction_command_log.mutable_version()->set_major(1);
  transaction_command_log.mutable_version()->set_minor(0);

  TransactionCommandLog_1_0 transaction_command_log_1_0;
  transaction_command_log_1_0.set_type(CommandType::COMMAND_TYPE_UNKNOWN);
  transaction_command_log_1_0.set_log_body("1");
  auto size = transaction_command_log_1_0.ByteSizeLong();
  BytesBuffer bytes_buffer_1_0(size);
  transaction_command_log_1_0.SerializeToArray(bytes_buffer_1_0.bytes->data(),
                                               size);

  transaction_command_log.set_log_body(bytes_buffer_1_0.bytes->data(), size);

  Uuid transaction_id = Uuid::GenerateUuid();
  size = transaction_command_log.ByteSizeLong();
  BytesBuffer bytes_buffer(size);
  transaction_command_log.SerializeToArray(bytes_buffer.bytes->data(), size);
  bytes_buffer.length = size;

  EXPECT_EQ(
      transaction_command_serializer.Deserialize(transaction_id, bytes_buffer,
                                                 transaction_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE));
}

TEST(TransactionCommandSerializerTest, InvalidConsumeBudgetCommandLogType) {
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<TransactionCommand> transaction_command =
      make_shared<TransactionCommand>();

  TransactionCommandSerializer transaction_command_serializer(
      async_executor, budget_key_provider);

  TransactionCommandLog transaction_command_log;
  transaction_command_log.mutable_version()->set_major(1);
  transaction_command_log.mutable_version()->set_minor(0);

  TransactionCommandLog_1_0 transaction_command_log_1_0;
  transaction_command_log_1_0.set_type(CommandType::CONSUME_BUDGET_COMMAND_1_0);
  auto size = transaction_command_log_1_0.ByteSizeLong();
  BytesBuffer bytes_buffer_1_0(size);
  transaction_command_log_1_0.SerializeToArray(bytes_buffer_1_0.bytes->data(),
                                               size);

  transaction_command_log.set_log_body(bytes_buffer_1_0.bytes->data(), size);

  Uuid transaction_id = Uuid::GenerateUuid();
  size = transaction_command_log.ByteSizeLong();
  BytesBuffer bytes_buffer(size);
  transaction_command_log.SerializeToArray(bytes_buffer.bytes->data(), size);
  bytes_buffer.length = size;

  EXPECT_EQ(
      transaction_command_serializer.Deserialize(transaction_id, bytes_buffer,
                                                 transaction_command),
      FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED));
}

TEST(TransactionCommandSerializerTest, ConsumeBudgetTransactionCommand_1_0) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  ConsumeBudgetCommandRequestInfo budget_consumption(100, 24, 1);
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  shared_ptr<TransactionCommand> consume_budget_command =
      make_shared<ConsumeBudgetCommand>(transaction_id, budget_key_name,
                                        move(budget_consumption),
                                        async_executor, budget_key_provider);

  TransactionCommandSerializer transaction_command_serializer(
      async_executor, budget_key_provider);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(transaction_command_serializer.Serialize(
                transaction_id, consume_budget_command, bytes_buffer),
            SuccessExecutionResult());

  shared_ptr<TransactionCommand> deserialized_consume_budget_command;
  EXPECT_EQ(
      transaction_command_serializer.Deserialize(
          transaction_id, bytes_buffer, deserialized_consume_budget_command),
      SuccessExecutionResult());

  auto old_consume_budget_command =
      static_pointer_cast<ConsumeBudgetCommand>(consume_budget_command);
  auto new_consume_budget_command = static_pointer_cast<ConsumeBudgetCommand>(
      deserialized_consume_budget_command);
  EXPECT_EQ(old_consume_budget_command->command_id, kConsumeBudgetCommandId);
  EXPECT_EQ(new_consume_budget_command->command_id, kConsumeBudgetCommandId);
  EXPECT_EQ(*new_consume_budget_command->GetBudgetKeyName(),
            *old_consume_budget_command->GetBudgetKeyName());
  EXPECT_EQ(new_consume_budget_command->GetTimeBucket(),
            old_consume_budget_command->GetTimeBucket());
  EXPECT_EQ(new_consume_budget_command->GetTokenCount(),
            old_consume_budget_command->GetTokenCount());
  EXPECT_EQ(new_consume_budget_command->GetVersion(),
            old_consume_budget_command->GetVersion());
  EXPECT_FALSE(new_consume_budget_command->GetRequestIndex().has_value());
}

TEST(TransactionCommandSerializerTest, ConsumeBudgetTransactionCommand_1_1) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  ConsumeBudgetCommandRequestInfo budget_consumption(100, 20, 123);
  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  shared_ptr<TransactionCommand> consume_budget_command =
      make_shared<ConsumeBudgetCommand>(transaction_id, budget_key_name,
                                        move(budget_consumption),
                                        async_executor, budget_key_provider);

  TransactionCommandSerializer transaction_command_serializer(
      async_executor, budget_key_provider,
      TransactionCommandSerializer::ConsumeBudgetCommandVersion::Version_1_1);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(transaction_command_serializer.Serialize(
                transaction_id, consume_budget_command, bytes_buffer),
            SuccessExecutionResult());

  shared_ptr<TransactionCommand> deserialized_consume_budget_command;
  EXPECT_EQ(
      transaction_command_serializer.Deserialize(
          transaction_id, bytes_buffer, deserialized_consume_budget_command),
      SuccessExecutionResult());

  auto old_consume_budget_command =
      static_pointer_cast<ConsumeBudgetCommand>(consume_budget_command);
  auto new_consume_budget_command = static_pointer_cast<ConsumeBudgetCommand>(
      deserialized_consume_budget_command);
  EXPECT_EQ(old_consume_budget_command->command_id, kConsumeBudgetCommandId);
  EXPECT_EQ(new_consume_budget_command->command_id, kConsumeBudgetCommandId);
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
  EXPECT_TRUE(new_consume_budget_command->GetRequestIndex().has_value());
  EXPECT_TRUE(old_consume_budget_command->GetRequestIndex().has_value());
  EXPECT_EQ(new_consume_budget_command->GetRequestIndex().value(), 123);
  EXPECT_EQ(old_consume_budget_command->GetRequestIndex().value(), 123);
}

TEST(TransactionCommandSerializerTest, BatchConsumeBudgetTransactionCommand) {
  Uuid transaction_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
  budget_consumptions.emplace_back(100, 2);
  budget_consumptions.emplace_back(200, 4);

  shared_ptr<BudgetKeyProviderInterface> budget_key_provider;
  shared_ptr<AsyncExecutorInterface> async_executor;

  shared_ptr<TransactionCommand> batch_consume_budget_command =
      make_shared<BatchConsumeBudgetCommand>(
          transaction_id, budget_key_name, move(budget_consumptions),
          async_executor, budget_key_provider);

  TransactionCommandSerializer transaction_command_serializer(
      async_executor, budget_key_provider);

  BytesBuffer bytes_buffer;
  EXPECT_EQ(transaction_command_serializer.Serialize(
                transaction_id, batch_consume_budget_command, bytes_buffer),
            SuccessExecutionResult());

  shared_ptr<TransactionCommand> deserialized_consume_budget_command;
  EXPECT_EQ(
      transaction_command_serializer.Deserialize(
          transaction_id, bytes_buffer, deserialized_consume_budget_command),
      SuccessExecutionResult());

  auto old_batch_consume_budget_command =
      static_pointer_cast<BatchConsumeBudgetCommand>(
          batch_consume_budget_command);
  auto new_batch_consume_budget_command =
      static_pointer_cast<BatchConsumeBudgetCommand>(
          deserialized_consume_budget_command);
  EXPECT_EQ(old_batch_consume_budget_command->command_id,
            kBatchConsumeBudgetCommandId);
  EXPECT_EQ(new_batch_consume_budget_command->command_id,
            kBatchConsumeBudgetCommandId);
  EXPECT_EQ(*new_batch_consume_budget_command->GetBudgetKeyName(),
            *old_batch_consume_budget_command->GetBudgetKeyName());
  EXPECT_EQ(new_batch_consume_budget_command->GetBudgetConsumptions().size(),
            2);
  EXPECT_EQ(
      new_batch_consume_budget_command->GetBudgetConsumptions()[0].time_bucket,
      100);
  EXPECT_EQ(
      new_batch_consume_budget_command->GetBudgetConsumptions()[0].token_count,
      2);
  EXPECT_EQ(
      new_batch_consume_budget_command->GetBudgetConsumptions()[1].time_bucket,
      200);
  EXPECT_EQ(
      new_batch_consume_budget_command->GetBudgetConsumptions()[1].token_count,
      4);
  EXPECT_EQ(new_batch_consume_budget_command->GetBudgetConsumptions(),
            old_batch_consume_budget_command->GetBudgetConsumptions());
  EXPECT_EQ(new_batch_consume_budget_command->GetVersion(),
            old_batch_consume_budget_command->GetVersion());
}

}  // namespace google::scp::pbs::test
