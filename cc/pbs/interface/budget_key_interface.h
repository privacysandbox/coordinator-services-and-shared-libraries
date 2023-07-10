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

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/service_interface.h"
#include "core/interface/transaction_protocol_interface.h"

#include "type_def.h"

namespace google::scp::pbs {

struct BudgetConsumptionRequestInfo {
  /**
   * @brief Time bucket to use for the consume budget operation. A budget key
   * includes multiple time buckets for every 1 hour of a day. This variable
   * must be encoded in time format and floored to the nearest 1 hour.
   */
  TimeBucket time_bucket = 0;
  /// Number of tokens to consume in the time bucket
  TokenCount token_count = 0;
};

/// The request object for the load budget key operation.
struct LoadBudgetKeyRequest {};

/// The response object for the load budget key operation.
struct LoadBudgetKeyResponse {};

/// The request object for the get budget operation.
struct GetBudgetRequest {
  /// Transaction id associated with the request.
  core::common::Uuid transaction_id = core::common::kZeroUuid;
  /**
   * @brief A budget key includes multiple time buckets for every 1 hour of a
   * day. This variable must be encoded in time format and floored to the
   * nearest 1 hour.
   */
  TimeBucket time_bucket = 0;
};

/// The response object for the get budget operation.
struct GetBudgetResponse {
  /// The remaining token counts for a specifc time range.
  TokenCount token_count = 0;
};

/**
 * @brief The request object for the prepare consume budget request. The caller
 * must provide the time bucket and token count that needs to be consumed.
 */
struct PrepareConsumeBudgetRequest {
  /// Transaction id associated with the request.
  core::common::Uuid transaction_id = core::common::kZeroUuid;
  /**
   * @brief Time bucket to use for the consume budget operation. A budget key
   * includes multiple time buckets for every 1 hour of a day. This variable
   * must be encoded in time format and floored to the nearest 1 hour.
   */
  TimeBucket time_bucket = 0;
  /// Total token counts to be taken from the time bucket budgets.
  TokenCount token_count = 0;
};

/// The response object for the prepare consume budget request.
struct PrepareConsumeBudgetResponse {};

/**
 * @brief The request object for the prepare batch consume budget request. The
 * caller must provide the time buckets and token counts that needs to be
 * consumed.
 */
struct PrepareBatchConsumeBudgetRequest {
  /// Transaction id associated with the request.
  core::common::Uuid transaction_id = core::common::kZeroUuid;
  /// Budgets to consume, ordered w.r.t. time buckets
  std::vector<BudgetConsumptionRequestInfo> budget_consumptions;
};

/// The response object for the prepare consume budget request.
struct PrepareBatchConsumeBudgetResponse {
  /// Indices of the budgets from 'PrepareBatchConsumeBudgetRequest' which
  /// failed due to insufficient budget
  std::vector<size_t> failed_budget_consumption_indices;
};

/**
 * @brief The request object for the commit consume budget request. The caller
 * must provide the time bucket and token count that needs to be consumed.
 */
struct CommitConsumeBudgetRequest {
  /// Transaction id associated with the request.
  core::common::Uuid transaction_id = core::common::kZeroUuid;
  /**
   * @brief Time bucket to use for the consume budget operation. A budget key
   * includes multiple time buckets for every 1 hour of a day. This variable
   * must be encoded in time format and floored to the nearest 1 hour.
   */
  TimeBucket time_bucket = 0;
  /// Total token counts to be taken from the time bucket budgets.
  TokenCount token_count = 0;
};

/// The response object for the commit consume budget request.
struct CommitConsumeBudgetResponse {};

/**
 * @brief The request object for the commit batch consume budget request. The
 * caller must provide the time buckets and token counts that needs to be
 * consumed.
 */
struct CommitBatchConsumeBudgetRequest {
  /// Transaction id associated with the request.
  core::common::Uuid transaction_id = core::common::kZeroUuid;
  /// Budgets to consume, ordered w.r.t. time buckets
  std::vector<BudgetConsumptionRequestInfo> budget_consumptions;
};

/// The response object for the commit consume budget request.
struct CommitBatchConsumeBudgetResponse {
  /// Indices of the budgets from 'CommitBatchConsumeBudgetRequest' which failed
  /// due to insufficient budget
  std::vector<size_t> failed_budget_consumption_indices;
};

/**
 * @brief The request object for the notify consume budget request. The caller
 * must provide the time bucket and the outcome of the operation.
 */
struct NotifyConsumeBudgetRequest {
  /// Transaction id associated with the request.
  core::common::Uuid transaction_id = core::common::kZeroUuid;
  /// Time bucket to use for the consume budget operation.
  TimeBucket time_bucket = 0;
};

/// The response object for the notify consume budget request.
struct NotifyConsumeBudgetResponse {};

/**
 * @brief The request object for the notify batch consume budget request. The
 * caller must provide the time buckets.
 */
struct NotifyBatchConsumeBudgetRequest {
  /// Transaction id associated with the request.
  core::common::Uuid transaction_id = core::common::kZeroUuid;
  /// Time buckets to use for the consume budget operation.
  std::vector<TimeBucket> time_buckets;
};

/// The response object for the notify consume budget request.
struct NotifyBatchConsumeBudgetResponse {};

/**
 * @brief The request object for the abort consume budget request. The caller
 * must provide the time bucket.
 */
struct AbortConsumeBudgetRequest {
  /// Transaction id associated with the request.
  core::common::Uuid transaction_id = core::common::kZeroUuid;
  /// Time bucket to use for the consume budget operation.
  TimeBucket time_bucket = 0;
};

/// The response object for the abort batch budget operation.
struct AbortConsumeBudgetResponse {};

/**
 * @brief The request object for the abort batch consume budget request. The
 * caller must provide the time buckets.
 */
struct AbortBatchConsumeBudgetRequest {
  /// Transaction id associated with the request.
  core::common::Uuid transaction_id = core::common::kZeroUuid;
  /// Time buckets to use for the consume budget operation.
  std::vector<TimeBucket> time_buckets;
};

/// The response object for the abort batch budget operation.
struct AbortBatchConsumeBudgetResponse {};

/// The interface for consuming budget transactionally.
class ConsumeBudgetTransactionProtocolInterface
    : public core::TransactionProtocolInterface<
          PrepareConsumeBudgetRequest, PrepareConsumeBudgetResponse,
          CommitConsumeBudgetRequest, CommitConsumeBudgetResponse,
          NotifyConsumeBudgetRequest, NotifyConsumeBudgetResponse,
          AbortConsumeBudgetRequest, AbortConsumeBudgetResponse> {
 public:
  virtual ~ConsumeBudgetTransactionProtocolInterface() = default;

  virtual core::ExecutionResult Prepare(
      core::AsyncContext<PrepareConsumeBudgetRequest,
                         PrepareConsumeBudgetResponse>&
          prepare_context) noexcept = 0;

  virtual core::ExecutionResult Commit(
      core::AsyncContext<CommitConsumeBudgetRequest,
                         CommitConsumeBudgetResponse>&
          commit_context) noexcept = 0;

  virtual core::ExecutionResult Notify(
      core::AsyncContext<NotifyConsumeBudgetRequest,
                         NotifyConsumeBudgetResponse>&
          notify_context) noexcept = 0;

  virtual core::ExecutionResult Abort(
      core::AsyncContext<AbortConsumeBudgetRequest, AbortConsumeBudgetResponse>&
          abort_context) noexcept = 0;
};

/// The interface for consuming several budgets transactionally.
class BatchConsumeBudgetTransactionProtocolInterface
    : public core::TransactionProtocolInterface<
          PrepareBatchConsumeBudgetRequest, PrepareBatchConsumeBudgetResponse,
          CommitBatchConsumeBudgetRequest, CommitBatchConsumeBudgetResponse,
          NotifyBatchConsumeBudgetRequest, NotifyBatchConsumeBudgetResponse,
          AbortBatchConsumeBudgetRequest, AbortBatchConsumeBudgetResponse> {
 public:
  virtual ~BatchConsumeBudgetTransactionProtocolInterface() = default;

  virtual core::ExecutionResult Prepare(
      core::AsyncContext<PrepareBatchConsumeBudgetRequest,
                         PrepareBatchConsumeBudgetResponse>&
          prepare_context) noexcept = 0;

  virtual core::ExecutionResult Commit(
      core::AsyncContext<CommitBatchConsumeBudgetRequest,
                         CommitBatchConsumeBudgetResponse>&
          commit_context) noexcept = 0;

  virtual core::ExecutionResult Notify(
      core::AsyncContext<NotifyBatchConsumeBudgetRequest,
                         NotifyBatchConsumeBudgetResponse>&
          notify_context) noexcept = 0;

  virtual core::ExecutionResult Abort(
      core::AsyncContext<AbortBatchConsumeBudgetRequest,
                         AbortBatchConsumeBudgetResponse>&
          abort_context) noexcept = 0;
};

/**
 * @brief Responsible to handle all the budget key operations such as
 * read/modify.
 */
class BudgetKeyInterface : public core::ServiceInterface {
 public:
  virtual ~BudgetKeyInterface() = default;

  /**
   * @brief Returns success execution result if the budget key can be cleanly
   * unloaded. This must be a sync operation.
   *
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult CanUnload() noexcept = 0;

  /**
   * @brief Loads the current budget key.
   *
   * @param load_budget_key_context the async context of load budget key
   * operation.
   * @return core::ExecutionResult the result of the operation.
   */
  virtual core::ExecutionResult LoadBudgetKey(
      core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
          load_budget_key_context) noexcept = 0;

  /**
   * @brief Gets the remaining budget for a specific time bucket. This is a read
   * operation and does not modify data.
   *
   * @param get_budget_context the async context of GetBudget operation.
   * @return core::ExecutionResult the result of the operation.
   */
  virtual core::ExecutionResult GetBudget(
      core::AsyncContext<GetBudgetRequest, GetBudgetResponse>&
          get_budget_context) noexcept = 0;

  /**
   * @brief Get the Budget Consumption Transaction Protocol object. The returned
   * object is then used to run transactions against the budget key object.
   *
   * @return const std::shared_ptr<core::TransactionProtocolInterface>
   */
  virtual const std::shared_ptr<ConsumeBudgetTransactionProtocolInterface>
  GetBudgetConsumptionTransactionProtocol() noexcept = 0;

  /**
   * @brief Get the Batch Budget Consumption Transaction Protocol object. The
   * returned object is then used to run transactions against the budget key
   * object.
   *
   * @return const std::shared_ptr<core::TransactionProtocolInterface>
   */
  virtual const std::shared_ptr<BatchConsumeBudgetTransactionProtocolInterface>
  GetBatchBudgetConsumptionTransactionProtocol() noexcept = 0;

  /**
   * @brief Returns the name of the budget key.
   *
   * @return const std::shared_ptr<BudgetKeyName> The budget key name.
   */
  virtual const std::shared_ptr<BudgetKeyName> GetName() noexcept = 0;

  /**
   * @brief Returns the id of the current budget key.
   *
   * @return const core::common::Uuid The budget key id.
   */
  virtual const core::common::Uuid GetId() noexcept = 0;

  /**
   * @brief Creates a checkpoint of the current transaction manager state.
   *
   * @param checkpoint_logs The vector of checkpoint metadata.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult Checkpoint(
      std::shared_ptr<std::list<core::CheckpointLog>>&
          checkpoint_logs) noexcept = 0;
};
}  // namespace google::scp::pbs
