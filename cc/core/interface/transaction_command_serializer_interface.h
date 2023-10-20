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

#include "transaction_manager_interface.h"
#include "type_def.h"

namespace google::scp::core {
/**
 * @brief Responsible to provide serialization and deserialization functionality
 * for the transaction commands.
 */
class TransactionCommandSerializerInterface {
 public:
  virtual ~TransactionCommandSerializerInterface() = default;

  /**
   * @brief Serializes a specific command to a byte array.
   *
   * @param transaction_id The transaction id performing the command
   * serialization.
   * @param transaction_command The transaction command to be serialized.
   * @param bytes_buffer The bytes buffer to write the serialized data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Serialize(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<TransactionCommand>& transaction_command,
      BytesBuffer& bytes_buffer) noexcept = 0;

  /**
   * @brief deserializes a specific command from a byte array.
   *
   * @param transaction_id The transaction id performing the command
   * deserialization.
   * @param bytes_buffer The bytes buffer to read the serialized data from.
   * @param transaction_command The transaction command to be deserialized.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Deserialize(
      const core::common::Uuid& transaction_id, const BytesBuffer& bytes_buffer,
      std::shared_ptr<TransactionCommand>& transaction_command) noexcept = 0;
};
}  // namespace google::scp::core
