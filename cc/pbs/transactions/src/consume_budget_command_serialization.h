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

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/transactions/src/consume_budget_command.h"
#include "pbs/transactions/src/proto/transaction_command.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::pbs {
// Provides serialization functionality for the consume budget command.
class ConsumeBudgetCommandSerialization {
 public:
  /**
   * @brief Serializes consume budget command into a bytes buffer.
   *
   * @param transaction_id The id of the transaction that is serializing the
   * consume budget command.
   * @param transaction_command The consume budget transaction command to be
   * serialized.
   * @param bytes_buffer The bytes buffer to write the serialized transaction
   * command to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeVersion_1_0(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<core::TransactionCommand>& transaction_command,
      core::BytesBuffer& bytes_buffer) noexcept {
    if (transaction_command->command_id != kConsumeBudgetCommandId) {
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE);
    }

    auto consume_budget_command =
        std::static_pointer_cast<ConsumeBudgetCommand>(transaction_command);

    pbs::transactions::proto::ConsumeBudgetCommand_1_0
        consume_budget_command_1_0;
    consume_budget_command_1_0.set_budget_key_name(
        *consume_budget_command->GetBudgetKeyName());
    consume_budget_command_1_0.set_token_count(
        consume_budget_command->GetTokenCount());
    consume_budget_command_1_0.set_time_bucket(
        consume_budget_command->GetTimeBucket());

    auto size = consume_budget_command_1_0.ByteSizeLong();
    bytes_buffer.bytes = std::make_shared<std::vector<core::Byte>>(size);
    bytes_buffer.capacity = size;
    bytes_buffer.length = size;

    if (!consume_budget_command_1_0.SerializeToArray(bytes_buffer.bytes->data(),
                                                     size)) {
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_SERIALIZATION_FAILED);
    }

    return core::SuccessExecutionResult();
  }

  /**
   * @brief Serializes consume budget command into a bytes buffer.
   *
   * @param transaction_id The id of the transaction that is serializing the
   * consume budget command.
   * @param transaction_command The consume budget transaction command to be
   * serialized.
   * @param bytes_buffer The bytes buffer to write the serialized transaction
   * command to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeVersion_1_1(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<core::TransactionCommand>& transaction_command,
      core::BytesBuffer& bytes_buffer) noexcept {
    if (transaction_command->command_id != kConsumeBudgetCommandId) {
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE);
    }

    auto consume_budget_command =
        std::static_pointer_cast<ConsumeBudgetCommand>(transaction_command);

    pbs::transactions::proto::ConsumeBudgetCommand_1_1
        consume_budget_command_1_1;
    consume_budget_command_1_1.set_budget_key_name(
        *consume_budget_command->GetBudgetKeyName());
    consume_budget_command_1_1.set_token_count(
        consume_budget_command->GetTokenCount());
    consume_budget_command_1_1.set_time_bucket(
        consume_budget_command->GetTimeBucket());
    if (consume_budget_command->GetRequestIndex().has_value()) {
      consume_budget_command_1_1.set_request_index(
          *consume_budget_command->GetRequestIndex());
    }

    auto size = consume_budget_command_1_1.ByteSizeLong();
    bytes_buffer.bytes = std::make_shared<std::vector<core::Byte>>(size);
    bytes_buffer.capacity = size;
    bytes_buffer.length = size;

    if (!consume_budget_command_1_1.SerializeToArray(bytes_buffer.bytes->data(),
                                                     size)) {
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_SERIALIZATION_FAILED);
    }

    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes consume budget command from a bytes buffer.
   *
   * @param transaction_id The id of the transaction that is deserializing the
   * consume budget command.
   * @param bytes_buffer The bytes buffer to read the serialized transaction
   * command from.
   * @param transaction_command The consume budget transaction command to
   * be deserialized.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeVersion_1_0(
      const core::common::Uuid& transaction_id,
      const core::BytesBuffer& bytes_buffer,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider,
      std::shared_ptr<core::TransactionCommand>& transaction_command) noexcept {
    pbs::transactions::proto::ConsumeBudgetCommand_1_0
        consume_budget_command_1_0;
    if (bytes_buffer.length == 0 ||
        !consume_budget_command_1_0.ParseFromArray(bytes_buffer.bytes->data(),
                                                   bytes_buffer.length)) {
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED);
    }

    auto budget_key_name = std::make_shared<BudgetKeyName>(
        consume_budget_command_1_0.budget_key_name());
    transaction_command = std::make_shared<ConsumeBudgetCommand>(
        transaction_id, budget_key_name,
        ConsumeBudgetCommandRequestInfo(
            consume_budget_command_1_0.time_bucket(),
            consume_budget_command_1_0.token_count()),
        async_executor, budget_key_provider);

    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes consume budget command from a bytes buffer.
   *
   * @param transaction_id The id of the transaction that is deserializing the
   * consume budget command.
   * @param bytes_buffer The bytes buffer to read the serialized transaction
   * command from.
   * @param transaction_command The consume budget transaction command to
   * be deserialized.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeVersion_1_1(
      const core::common::Uuid& transaction_id,
      const core::BytesBuffer& bytes_buffer,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider,
      std::shared_ptr<core::TransactionCommand>& transaction_command) noexcept {
    pbs::transactions::proto::ConsumeBudgetCommand_1_1
        consume_budget_command_1_1;
    if (bytes_buffer.length == 0 ||
        !consume_budget_command_1_1.ParseFromArray(bytes_buffer.bytes->data(),
                                                   bytes_buffer.length)) {
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED);
    }

    auto budget_key_name = std::make_shared<BudgetKeyName>(
        consume_budget_command_1_1.budget_key_name());
    auto budget_info = ConsumeBudgetCommandRequestInfo(
        consume_budget_command_1_1.time_bucket(),
        consume_budget_command_1_1.token_count());
    if (consume_budget_command_1_1.has_request_index()) {
      budget_info.request_index = consume_budget_command_1_1.request_index();
    }
    transaction_command = std::make_shared<ConsumeBudgetCommand>(
        transaction_id, budget_key_name, std::move(budget_info), async_executor,
        budget_key_provider);

    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::pbs
