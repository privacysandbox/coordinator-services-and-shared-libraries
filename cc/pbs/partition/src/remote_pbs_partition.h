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
#include <string>

#include "core/interface/partition_types.h"
#include "pbs/interface/pbs_partition_interface.h"

#include "error_codes.h"

namespace google::scp::pbs {
/**
 * @brief RemotePBSPartition
 *
 * This represents a partition that is not loaded on this node. Request must not
 * be send to this partition for processing, if sent will be returned with a
 * retriable error.
 */
class RemotePBSPartition : public PBSPartitionInterface {
 public:
  RemotePBSPartition()
      : partition_state_(core::PartitionLoadUnloadState::Created) {}

  core::ExecutionResult Init() noexcept override {
    partition_state_ = core::PartitionLoadUnloadState::Initialized;
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Load() noexcept override {
    partition_state_ = core::PartitionLoadUnloadState::Loaded;
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Unload() noexcept override {
    partition_state_ = core::PartitionLoadUnloadState::Unloaded;
    return core::SuccessExecutionResult();
  }

  core::PartitionLoadUnloadState GetPartitionState() noexcept override {
    return partition_state_.load();
  }

 public:
  core::ExecutionResult ExecuteRequest(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&) noexcept override {
    return core::RetryExecutionResult(
        core::errors::SC_PBS_PARTITION_IS_REMOTE_CANNOT_HANDLE_REQUEST);
  }

  core::ExecutionResult ExecuteRequest(
      core::AsyncContext<core::TransactionRequest,
                         core::TransactionResponse>&) noexcept override {
    return core::RetryExecutionResult(
        core::errors::SC_PBS_PARTITION_IS_REMOTE_CANNOT_HANDLE_REQUEST);
  }

  core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&) noexcept
      override {
    return core::RetryExecutionResult(
        core::errors::SC_PBS_PARTITION_IS_REMOTE_CANNOT_HANDLE_REQUEST);
  }

  core::ExecutionResult GetTransactionManagerStatus(
      const core::GetTransactionManagerStatusRequest&,
      core::GetTransactionManagerStatusResponse&) noexcept override {
    return core::RetryExecutionResult(
        core::errors::SC_PBS_PARTITION_IS_REMOTE_CANNOT_HANDLE_REQUEST);
  }

 protected:
  std::atomic<core::PartitionLoadUnloadState> partition_state_;
};
}  // namespace google::scp::pbs
