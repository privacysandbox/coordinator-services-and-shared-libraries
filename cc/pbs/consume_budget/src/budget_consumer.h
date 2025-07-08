// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CC_PBS_CONSUME_BUDGET_SRC_BUDGET_CONSUMER_H_
#define CC_PBS_CONSUME_BUDGET_SRC_BUDGET_CONSUMER_H_

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cc/core/interface/http_types.h"
#include "cc/public/core/interface/execution_result.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/results.h"
#include "proto/pbs/api/v1/api.pb.h"

namespace privacy_sandbox::pbs {

inline constexpr absl::string_view kBudgetTableBudgetKeyColumn = "Budget_Key";
inline constexpr absl::string_view kBudgetTableTimeframeColumn = "Timeframe";
inline constexpr absl::string_view kBudgetTableValueSpannerColumn = "Value";
inline constexpr absl::string_view kBudgetTableValueProtoColumn = "ValueProto";

inline constexpr absl::string_view kBudgetTypeBinaryBudget =
    "BUDGET_TYPE_BINARY_BUDGET";

// Results while trying to create spanner mutations from the related rows.
struct SpannerMutationsResult {
  google::cloud::Status status;
  pbs_common::ExecutionResult execution_result;
  std::vector<size_t> budget_exhausted_indices;
  google::cloud::spanner::Mutations mutations;
};

// A helper class to isolate budget consumption logic from the infrastructure
// logic.
class BudgetConsumer {
 public:
  virtual ~BudgetConsumer() = default;

  /**
   * @brief Parse the HTTP request headers and body to an internal state to be
   * later used by the BudgetConsumptionHelper.
   *
   * @param authorized_domain The authorized domain in HTTP auth headers.
   * @param request_headers The headers from the HTTP request.
   * @param request_proto The request proto derived from the HTTP request body.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual pbs_common::ExecutionResult ParseTransactionRequest(
      const pbs_common::AuthContext& auth_context,
      const pbs_common::HttpHeaders& request_headers,
      const privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest&
          request_proto) = 0;

  /**
    * @brief Returns the numbers of keys in the request

    * @return number of keys in the request
   */
  virtual size_t GetKeyCount() = 0;

  /**
   * @return google::cloud::spanner::KeySet Keys to query the database.
   */
  virtual google::cloud::spanner::KeySet GetSpannerKeySet() = 0;

  /**
   * @brief Read the data from Spanner via the row stream and spit out the
   * mutations.
   *
   * @param google::cloud::spanner::RowStream& The rows read from the database
   * @return SpannerMutationsResult mutations if they were created successfully.
   * error details if they weren't
   */
  virtual SpannerMutationsResult ConsumeBudget(
      google::cloud::spanner::RowStream&, absl::string_view table_name) = 0;

  /**
   * @brief Returns list of debug strings for all keys.
   *
   * @return Returns list of debug strings for all keys.
   */
  virtual std::vector<std::string> DebugKeyList() = 0;

  /**
   * @brief Returns the columns to read from the database. This is only used
   * during database migration for binary budget consumption.
   *
   * @return Returns the columns to read from the datababse
   */
  virtual pbs_common::ExecutionResultOr<std::vector<std::string>>
  GetReadColumns() {
    return std::vector<std::string>{
        std::string(kBudgetTableBudgetKeyColumn),
        std::string(kBudgetTableTimeframeColumn),
        std::string(kBudgetTableValueProtoColumn),
    };
  }
};

}  // namespace privacy_sandbox::pbs

#endif  // CC_PBS_CONSUME_BUDGET_SRC_BUDGET_CONSUMER_H_
