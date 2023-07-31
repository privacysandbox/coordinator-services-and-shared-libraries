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

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/transactions/src/proto/transaction_command.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs {
/*! @copydoc TransactionCommandSerializerInterface
 */
class TransactionCommandSerializer
    : public core::TransactionCommandSerializerInterface {
 public:
  /**
   * @brief Tracks different version types of Consume Budget Commands which can
   * be used for serialization of commands.
   */
  enum class ConsumeBudgetCommandVersion {
    Version_Unknown = 0,
    Version_1_0 = 1,
    Version_1_1 = 2
  };

  /**
   * @brief Tracks different version types of Batch Consume Budget Commands
   * which can be used for serialization of commands.
   */
  enum class BatchConsumeBudgetCommandVersion {
    Version_Unknown = 0,
    Version_1_0 = 1
  };

  /**
   * @brief Construct a new Transaction Command Serializer object
   *
   * Version specification for Batch and Non-Batch command.
   *
   * NOTE: Bump up the version of a command type only after the code (esp.
   * deserialization part of the version) is available on all the nodes in
   * production environment.
   *
   * @param async_executor
   * @param budget_key_provider
   * @param consume_budget_command_version_for_serialization
   * @param batch_consume_budget_command_version_for_serialization
   */
  TransactionCommandSerializer(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<BudgetKeyProviderInterface> budget_key_provider,
      ConsumeBudgetCommandVersion
          consume_budget_command_version_for_serialization =
              ConsumeBudgetCommandVersion::Version_1_0,
      BatchConsumeBudgetCommandVersion
          batch_consume_budget_command_version_for_serialization =
              BatchConsumeBudgetCommandVersion::Version_1_0)
      : async_executor_(async_executor),
        budget_key_provider_(budget_key_provider),
        consume_budget_command_version_for_serialization_(
            consume_budget_command_version_for_serialization),
        batch_consume_budget_command_version_for_serialization_(
            batch_consume_budget_command_version_for_serialization) {}

  core::ExecutionResult Serialize(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<core::TransactionCommand>& transaction_command,
      core::BytesBuffer& bytes_buffer) noexcept override;

  core::ExecutionResult Deserialize(const core::common::Uuid& transaction_id,
                                    const core::BytesBuffer& bytes_buffer,
                                    std::shared_ptr<core::TransactionCommand>&
                                        transaction_command) noexcept override;

 protected:
  /**
   * @brief Checks whether a transaction command is serializable.
   *
   * @param transaction_command The transaction command to serialize.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult CanSerialize(
      const std::shared_ptr<core::TransactionCommand>&
          transaction_command) noexcept;

  /**
   * @brief Checks whether a transaction command log is deserializable.
   *
   * @param transaction_command_log The transaction command log to deserialize.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult CanDeserialize(
      const pbs::transactions::proto::TransactionCommandLog&
          transaction_command_log) noexcept;

  /**
   * @brief Serialize ConsumeBudgetCommand to TransactionLog using the version
   * specified in the member variable
   * consume_budget_command_version_for_serialization_
   *
   * @param transaction_id
   * @param transaction_command
   * @param bytes_buffer
   * @return ExecutionResult
   */
  core::ExecutionResult SerializeConsumeBudgetCommandToTransactionLog(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<core::TransactionCommand>& transaction_command,
      transactions::proto::TransactionCommandLog_1_0& bytes_buffer) const;

  /**
   * @brief Serialize BatchConsumeBudgetCommand to TransactionLog using the
   * version specified in the member variable
   * batch_consume_budget_command_version_for_serialization_
   *
   * @param transaction_id
   * @param transaction_command
   * @param log
   * @return ExecutionResult
   */
  core::ExecutionResult SerializeBatchConsumeBudgetCommandToTransactionLog(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<core::TransactionCommand>& transaction_command,
      transactions::proto::TransactionCommandLog_1_0& log) const;

 private:
  /// An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance of the budget key provider.
  std::shared_ptr<BudgetKeyProviderInterface> budget_key_provider_;

  /// Consume Budget command version to use for serialization
  ConsumeBudgetCommandVersion consume_budget_command_version_for_serialization_;

  /// Batch Consume Budget command version to use for serialization
  BatchConsumeBudgetCommandVersion
      batch_consume_budget_command_version_for_serialization_;
};
}  // namespace google::scp::pbs
