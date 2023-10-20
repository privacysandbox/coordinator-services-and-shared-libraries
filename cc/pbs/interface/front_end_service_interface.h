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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs {

/// Represents metadata collection for a consume budget operation.
struct ConsumeBudgetMetadata {
  /// The budget key name.
  std::shared_ptr<BudgetKeyName> budget_key_name;
  /// The token count to be consumed.
  TokenCount token_count = 0;
  /// The time bucket to consume the token.
  TimeBucket time_bucket = 0;
};

/// Represents consume budget transaction request object.
struct ConsumeBudgetTransactionRequest {
  /// Id of the transaction.
  core::common::Uuid transaction_id;
  /// In the case of remote transaction, the transaction secret will provide
  /// other participants to inquiry or update the state of a transaction.
  std::shared_ptr<std::string> transaction_secret;
  /// All the budget keys in the transaction.
  std::shared_ptr<std::vector<ConsumeBudgetMetadata>> budget_keys;
};

/// Represents consume budget transaction response object.
struct ConsumeBudgetTransactionResponse {
  /// The last execution time stamp of any phases of a transaction. This will
  /// provide optimistic concurrency behavior for the transaction execution.
  core::Timestamp last_execution_timestamp;
};

/**
 * @brief Responsible to provide front end functionality to the PBS service.
 * Front end layer is responsible to accept the traffic, validate the requests,
 * and then execute operations on behalf of the caller.
 */
class FrontEndServiceInterface : public core::ServiceInterface {
 public:
  virtual ~FrontEndServiceInterface() = default;

  /**
   * @brief Validates, constructs, and executes a consume budget transaction
   * request.
   *
   * @param consume_budget_transaction_context The consume budget transaction
   * context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  virtual core::ExecutionResult ExecuteConsumeBudgetTransaction(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context) noexcept = 0;
};

}  // namespace google::scp::pbs
