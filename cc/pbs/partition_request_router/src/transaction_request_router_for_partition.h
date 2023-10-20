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

#include "core/interface/partition_manager_interface.h"
#include "core/interface/partition_namespace_interface.h"
#include "core/interface/partition_types.h"
#include "core/interface/transaction_request_router_interface.h"
#include "pbs/interface/pbs_partition_interface.h"
#include "pbs/interface/pbs_partition_manager_interface.h"

namespace google::scp::pbs {
/**
 * @copydoc TransactionRequestRouterInterface
 * Implementation of TransactionRequestRouterInterface interface to route a
 * given transaction request type, to a target partition that can handle it.
 *
 * Target partition is determined with the help of ReportingOrigin of the
 * transaction request.
 */
class TransactionRequestRouterForPartition
    : public core::TransactionRequestRouterInterface {
 public:
  TransactionRequestRouterForPartition(
      const std::shared_ptr<core::PartitionNamespaceInterface>&
          partition_namespace,
      const std::shared_ptr<PBSPartitionManagerInterface>& partition_manager)
      : partition_namespace_(partition_namespace),
        partition_manager_(partition_manager) {}

  core::ExecutionResult Execute(
      core::AsyncContext<core::TransactionRequest, core::TransactionResponse>&
          context) noexcept override;

  core::ExecutionResult Execute(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>& context) noexcept
      override;

  core::ExecutionResult Execute(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>& context) noexcept
      override;

  /**
   * @brief This is an aggregate of all partition's Transaction Managers
   * statuses.
   *
   * @param request
   * @param response
   * @return core::ExecutionResult
   */
  core::ExecutionResult Execute(
      const core::GetTransactionManagerStatusRequest& request,
      core::GetTransactionManagerStatusResponse& response) noexcept override;

 protected:
  /**
   * @brief Get the Partition object
   *
   * @param resource_id
   * @return ExecutionResultOr<std::shared_ptr<PBSPartitionInterface>>
   */
  core::ExecutionResultOr<std::shared_ptr<PBSPartitionInterface>> GetPartition(
      const core::ResourceId& resource_id);

  /// @brief Namespace of the partition to which request will be mapped to.
  const std::shared_ptr<core::PartitionNamespaceInterface> partition_namespace_;

  /// @brief Partition object to be retrieved from
  const std::shared_ptr<PBSPartitionManagerInterface> partition_manager_;
};
}  // namespace google::scp::pbs
