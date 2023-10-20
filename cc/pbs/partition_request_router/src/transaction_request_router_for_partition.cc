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

#include "pbs/partition_request_router/src/transaction_request_router_for_partition.h"

#include "pbs/partition_request_router/src/error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionManagerStatusRequest;
using google::scp::core::GetTransactionManagerStatusResponse;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::PartitionId;
using google::scp::core::ResourceId;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::common::Uuid;

namespace google::scp::pbs {

ExecutionResultOr<std::shared_ptr<PBSPartitionInterface>>
TransactionRequestRouterForPartition::GetPartition(
    const ResourceId& resource_id) {
  auto partition_id = partition_namespace_->MapResourceToPartition(resource_id);
  return partition_manager_->GetPBSPartition(partition_id);
}

ExecutionResult TransactionRequestRouterForPartition::Execute(
    AsyncContext<TransactionRequest, TransactionResponse>& context) noexcept {
  if (!context.request->transaction_origin ||
      context.request->transaction_origin->empty()) {
    return FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID);
  }
  auto partition_or = GetPartition(*(context.request->transaction_origin));
  if (!partition_or.Successful()) {
    return FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_PARTITION_UNAVAILABLE);
  }
  return (*partition_or)->ExecuteRequest(context);
}

ExecutionResult TransactionRequestRouterForPartition::Execute(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        context) noexcept {
  if (!context.request->transaction_origin ||
      context.request->transaction_origin->empty()) {
    return FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID);
  }
  auto partition_or = GetPartition(*(context.request->transaction_origin));
  if (!partition_or.Successful()) {
    return FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_PARTITION_UNAVAILABLE);
  }
  return (*partition_or)->ExecuteRequest(context);
}

ExecutionResult TransactionRequestRouterForPartition::Execute(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        context) noexcept {
  auto partition_or = GetPartition(*(context.request->transaction_origin));
  if (!partition_or.Successful()) {
    return FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_PARTITION_UNAVAILABLE);
  }
  return (*partition_or)->GetTransactionStatus(context);
}

ExecutionResult TransactionRequestRouterForPartition::Execute(
    const GetTransactionManagerStatusRequest& request,
    GetTransactionManagerStatusResponse& response) noexcept {
  auto& partition_ids = partition_namespace_->GetPartitions();
  // Aggregate responses from each participating partition
  size_t pending_transactions_count = 0;
  for (const auto& partition_id : partition_ids) {
    auto partition_or = partition_manager_->GetPBSPartition(partition_id);
    if (!partition_or.Successful()) {
      // Partition may not be hosted, go to next partition.
      if (partition_or.result() ==
          FailureExecutionResult(
              core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)) {
        continue;
      }
      return partition_or.result();
    }
    GetTransactionManagerStatusResponse partition_get_status_response;
    RETURN_IF_FAILURE((*partition_or)
                          ->GetTransactionManagerStatus(
                              request, partition_get_status_response));
    pending_transactions_count +=
        partition_get_status_response.pending_transactions_count;
  }

  response.pending_transactions_count = pending_transactions_count;
  return SuccessExecutionResult();
}

}  // namespace google::scp::pbs
