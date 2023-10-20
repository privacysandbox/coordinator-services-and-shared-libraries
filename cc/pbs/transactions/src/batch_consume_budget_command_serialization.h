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
#include "pbs/transactions/src/batch_consume_budget_command.h"
#include "pbs/transactions/src/proto/transaction_command.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::pbs {
// Provides serialization functionality for the batch consume budget command.
class BatchConsumeBudgetCommandSerialization {
 public:
  /**
   * @brief Serializes batch consume budget command into a bytes buffer.
   *
   * @param transaction_id The id of the transaction that is serializing the
   * batch consume budget command.
   * @param transaction_command The batch consume budget transaction command to
   * be serialized.
   * @param bytes_buffer The bytes buffer to write the serialized transaction
   * command to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeVersion_1_0(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<core::TransactionCommand>& transaction_command,
      core::BytesBuffer& bytes_buffer) noexcept {
    if (transaction_command->command_id != kBatchConsumeBudgetCommandId) {
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_INVALID_COMMAND_TYPE);
    }

    auto batch_consume_budget_command =
        std::static_pointer_cast<BatchConsumeBudgetCommand>(
            transaction_command);

    pbs::transactions::proto::BatchConsumeBudgetCommand_1_0
        batch_consume_budget_command_1_0;

    batch_consume_budget_command_1_0.set_budget_key_name(
        *batch_consume_budget_command->GetBudgetKeyName());

    auto budget_consumptions =
        batch_consume_budget_command->GetBudgetConsumptions();
    for (const auto& budget_consumption : budget_consumptions) {
      auto budget_consumption_command_1_0 =
          batch_consume_budget_command_1_0.add_budget_consumptions();
      budget_consumption_command_1_0->set_time_bucket(
          budget_consumption.time_bucket);
      budget_consumption_command_1_0->set_token_count(
          budget_consumption.token_count);
      if (budget_consumption.request_index.has_value()) {
        budget_consumption_command_1_0->set_request_index(
            *budget_consumption.request_index);
      }
    }

    auto size = batch_consume_budget_command_1_0.ByteSizeLong();
    bytes_buffer.bytes = std::make_shared<std::vector<core::Byte>>(size);
    bytes_buffer.capacity = size;
    bytes_buffer.length = size;

    if (!batch_consume_budget_command_1_0.SerializeToArray(
            bytes_buffer.bytes->data(), size)) {
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_SERIALIZATION_FAILED);
    }

    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes batch consume budget command from a bytes buffer.
   *
   * @param transaction_id The id of the transaction that is deserializing the
   * batch consume budget command.
   * @param bytes_buffer The bytes buffer to read the serialized transaction
   * command from.
   * @param transaction_command The batch consume budget transaction command to
   * be deserialized.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeVersion_1_0(
      const core::common::Uuid& transaction_id,
      const core::BytesBuffer& bytes_buffer,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::shared_ptr<BudgetKeyProviderInterface>& budget_key_provider,
      std::shared_ptr<core::TransactionCommand>& transaction_command) noexcept {
    pbs::transactions::proto::BatchConsumeBudgetCommand_1_0
        batch_consume_budget_command_1_0;
    if (bytes_buffer.length == 0 ||
        !batch_consume_budget_command_1_0.ParseFromArray(
            bytes_buffer.bytes->data(), bytes_buffer.length)) {
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_COMMAND_SERIALIZER_DESERIALIZATION_FAILED);
    }

    auto budget_key_name = std::make_shared<BudgetKeyName>(
        batch_consume_budget_command_1_0.budget_key_name());

    std::vector<ConsumeBudgetCommandRequestInfo> budget_consumptions;
    for (const auto& budget_consumption :
         batch_consume_budget_command_1_0.budget_consumptions()) {
      if (budget_consumption.has_request_index()) {
        budget_consumptions.emplace_back(budget_consumption.time_bucket(),
                                         budget_consumption.token_count(),
                                         budget_consumption.request_index());
      } else {
        budget_consumptions.emplace_back(budget_consumption.time_bucket(),
                                         budget_consumption.token_count());
      }
    }

    transaction_command = std::make_shared<BatchConsumeBudgetCommand>(
        transaction_id, budget_key_name, move(budget_consumptions),
        async_executor, budget_key_provider);

    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::pbs
