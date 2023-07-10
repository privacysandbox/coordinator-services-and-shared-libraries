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

#include "transaction_command_serializer.h"

#include <memory>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "pbs/transactions/src/batch_consume_budget_command.h"
#include "pbs/transactions/src/batch_consume_budget_command_serialization.h"
#include "pbs/transactions/src/consume_budget_command.h"
#include "pbs/transactions/src/consume_budget_command_serialization.h"
#include "pbs/transactions/src/proto/transaction_command.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionCommand;
using google::scp::core::Version;
using google::scp::core::common::Uuid;
using google::scp::pbs::ConsumeBudgetCommand;
using google::scp::pbs::transactions::proto::CommandType;
using google::scp::pbs::transactions::proto::TransactionCommandLog;
using google::scp::pbs::transactions::proto::TransactionCommandLog_1_0;
using std::make_shared;
using std::shared_ptr;
using std::static_pointer_cast;
using std::vector;

static constexpr Version kCurrentVersion = {.major = 1, .minor = 0};

namespace google::scp::pbs {

ExecutionResult TransactionCommandSerializer::CanSerialize(
    const shared_ptr<TransactionCommand>& transaction_command) noexcept {
  if ((transaction_command->command_id != kConsumeBudgetCommandId) &&
      (transaction_command->command_id != kBatchConsumeBudgetCommandId)) {
    return FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_COMMAND_SERIALIZER_UNSUPPORTED);
  }

  return SuccessExecutionResult();
}

ExecutionResult TransactionCommandSerializer::CanDeserialize(
    const TransactionCommandLog& transaction_command_log) noexcept {
  if (transaction_command_log.version().major() != kCurrentVersion.major ||
      transaction_command_log.version().minor() != kCurrentVersion.minor) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_VERSION);
  }

  return SuccessExecutionResult();
}

ExecutionResult
TransactionCommandSerializer::SerializeConsumeBudgetCommandToTransactionLog(
    const Uuid& transaction_id, const shared_ptr<TransactionCommand>& command,
    TransactionCommandLog_1_0& log) const {
  BytesBuffer buffer;
  CommandType command_type = CommandType::COMMAND_TYPE_UNKNOWN;
  ExecutionResult execution_result;
  switch (consume_budget_command_version_for_serialization_) {
    case ConsumeBudgetCommandVersion::Version_1_1:
      command_type = CommandType::CONSUME_BUDGET_COMMAND_1_1;
      execution_result =
          ConsumeBudgetCommandSerialization::SerializeVersion_1_1(
              transaction_id, command, buffer);
      break;
    case ConsumeBudgetCommandVersion::Version_1_0:
      command_type = CommandType::CONSUME_BUDGET_COMMAND_1_0;
      execution_result =
          ConsumeBudgetCommandSerialization::SerializeVersion_1_0(
              transaction_id, command, buffer);
      break;
    default:
      execution_result = FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE);
  }

  if (!execution_result.Successful()) {
    return execution_result;
  }

  log.set_type(command_type);
  log.set_log_body(buffer.bytes->data(), buffer.length);

  return execution_result;
}

ExecutionResult TransactionCommandSerializer::
    SerializeBatchConsumeBudgetCommandToTransactionLog(
        const Uuid& transaction_id,
        const shared_ptr<TransactionCommand>& transaction_command,
        TransactionCommandLog_1_0& log) const {
  // Add other versions of BatchConsumeBudget commands if needed.
  if (batch_consume_budget_command_version_for_serialization_ !=
      BatchConsumeBudgetCommandVersion::Version_1_0) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_VERSION);
  }

  BytesBuffer buffer;
  CommandType command_type = CommandType::BATCH_CONSUME_BUDGET_COMMAND_1_0;

  auto execution_result =
      BatchConsumeBudgetCommandSerialization::SerializeVersion_1_0(
          transaction_id, transaction_command, buffer);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  log.set_type(command_type);
  log.set_log_body(buffer.bytes->data(), buffer.length);

  return execution_result;
}

ExecutionResult TransactionCommandSerializer::Serialize(
    const Uuid& transaction_id,
    const shared_ptr<TransactionCommand>& transaction_command,
    BytesBuffer& bytes_buffer) noexcept {
  TransactionCommandLog transaction_command_log;
  transaction_command_log.mutable_version()->set_major(kCurrentVersion.major);
  transaction_command_log.mutable_version()->set_minor(kCurrentVersion.minor);

  BytesBuffer command_bytes_buffer;
  auto execution_result = CanSerialize(transaction_command);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  TransactionCommandLog_1_0 transaction_command_log_1_0;
  if (transaction_command->command_id == kConsumeBudgetCommandId) {
    execution_result = SerializeConsumeBudgetCommandToTransactionLog(
        transaction_id, transaction_command, transaction_command_log_1_0);
  } else if (transaction_command->command_id == kBatchConsumeBudgetCommandId) {
    execution_result = SerializeBatchConsumeBudgetCommandToTransactionLog(
        transaction_id, transaction_command, transaction_command_log_1_0);
  }

  if (!execution_result.Successful()) {
    return execution_result;
  }

  auto size = transaction_command_log_1_0.ByteSizeLong();
  BytesBuffer log_bytes_buffer;
  log_bytes_buffer.capacity = size;
  log_bytes_buffer.length = size;
  log_bytes_buffer.bytes = make_shared<vector<Byte>>(size);

  if (!transaction_command_log_1_0.SerializeToArray(
          log_bytes_buffer.bytes->data(), size)) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_TRANSACTION_COMMAND_SERIALIZER_SERIALIZATION_FAILED);
  }

  transaction_command_log.set_log_body(log_bytes_buffer.bytes->data(),
                                       log_bytes_buffer.length);

  size = transaction_command_log.ByteSizeLong();
  bytes_buffer.capacity = size;
  bytes_buffer.length = size;
  bytes_buffer.bytes = make_shared<vector<Byte>>(size);

  if (!transaction_command_log.SerializeToArray(bytes_buffer.bytes->data(),
                                                size)) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_TRANSACTION_COMMAND_SERIALIZER_SERIALIZATION_FAILED);
  }

  return SuccessExecutionResult();
}

ExecutionResult TransactionCommandSerializer::Deserialize(
    const Uuid& transaction_id, const BytesBuffer& bytes_buffer,
    shared_ptr<TransactionCommand>& transaction_command) noexcept {
  TransactionCommandLog transaction_command_log;
  if (bytes_buffer.length == 0 ||
      !transaction_command_log.ParseFromArray(bytes_buffer.bytes->data(),
                                              bytes_buffer.length)) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_TRANSACTION_LOG);
  }

  auto execution_result = CanDeserialize(transaction_command_log);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  TransactionCommandLog_1_0 transaction_command_log_1_0;
  if (transaction_command_log.log_body().length() == 0 ||
      !transaction_command_log_1_0.ParseFromArray(
          transaction_command_log.log_body().c_str(),
          transaction_command_log.log_body().length())) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED);
  }

  auto size = transaction_command_log_1_0.log_body().length();
  BytesBuffer command_bytes_buffer;
  command_bytes_buffer.bytes =
      make_shared<vector<Byte>>(transaction_command_log_1_0.log_body().begin(),
                                transaction_command_log_1_0.log_body().end());
  command_bytes_buffer.capacity = size;
  command_bytes_buffer.length = size;

  switch (transaction_command_log_1_0.type()) {
    case CommandType::CONSUME_BUDGET_COMMAND_1_1:
      return ConsumeBudgetCommandSerialization::DeserializeVersion_1_1(
          transaction_id, command_bytes_buffer, async_executor_,
          budget_key_provider_, transaction_command);
    case CommandType::CONSUME_BUDGET_COMMAND_1_0:
      return ConsumeBudgetCommandSerialization::DeserializeVersion_1_0(
          transaction_id, command_bytes_buffer, async_executor_,
          budget_key_provider_, transaction_command);
    case CommandType::BATCH_CONSUME_BUDGET_COMMAND_1_0:
      return BatchConsumeBudgetCommandSerialization::DeserializeVersion_1_0(
          transaction_id, command_bytes_buffer, async_executor_,
          budget_key_provider_, transaction_command);
    default:
      return FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE);
  }

  return FailureExecutionResult(
      core::errors::SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE);
}

}  // namespace google::scp::pbs
