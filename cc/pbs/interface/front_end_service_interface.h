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
#include <vector>

#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/service_interface.h"
#include "cc/pbs/interface/type_def.h"

namespace google::scp::pbs {

/// Represents metadata collection for a consume budget operation.
struct ConsumeBudgetMetadata {
  /// The budget key name.
  std::shared_ptr<BudgetKeyName> budget_key_name;
  /// The token count to be consumed.
  TokenCount token_count = 0;
  /// The time bucket to consume the token.
  TimeBucket time_bucket = 0;

  std::string DebugString() const {
    return absl::StrFormat(
        "Budget Key: %s Reporting Time Bucket: %llu Token Count: %d",
        *budget_key_name, time_bucket, token_count);
  }
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
  privacy_sandbox::pbs_common::Timestamp last_execution_timestamp;
};

/**
 * @brief Responsible to provide front end functionality to the PBS service.
 * Front end layer is responsible to accept the traffic, validate the requests,
 * and then execute operations on behalf of the caller.
 */
class FrontEndServiceInterface
    : public privacy_sandbox::pbs_common::ServiceInterface {
 public:
  virtual ~FrontEndServiceInterface() = default;
};

}  // namespace google::scp::pbs
