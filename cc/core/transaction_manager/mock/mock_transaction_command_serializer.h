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

#include <atomic>
#include <functional>
#include <memory>

#include "core/interface/transaction_command_serializer_interface.h"

namespace google::scp::core::transaction_manager::mock {
class MockTransactionCommandSerializer
    : public core::TransactionCommandSerializerInterface {
 public:
  core::ExecutionResult Serialize(
      const core::common::Uuid& transaction_id,
      const std::shared_ptr<TransactionCommand>& transaction_command,
      BytesBuffer& bytes_buffer) noexcept override {
    if (serialize_mock) {
      return serialize_mock(transaction_id, transaction_command, bytes_buffer);
    }
    return SuccessExecutionResult();
  }

  core::ExecutionResult Deserialize(const core::common::Uuid& transaction_id,
                                    const BytesBuffer& bytes_buffer,
                                    std::shared_ptr<TransactionCommand>&
                                        transaction_command) noexcept override {
    if (deserialize_mock) {
      return deserialize_mock(transaction_id, bytes_buffer,
                              transaction_command);
    }
    return SuccessExecutionResult();
  }

  std::function<core::ExecutionResult(
      const core::common::Uuid&, const std::shared_ptr<TransactionCommand>&,
      BytesBuffer&)>
      serialize_mock;

  std::function<core::ExecutionResult(const core::common::Uuid&,
                                      const BytesBuffer&,
                                      std::shared_ptr<TransactionCommand>&)>
      deserialize_mock;
};
}  // namespace google::scp::core::transaction_manager::mock
