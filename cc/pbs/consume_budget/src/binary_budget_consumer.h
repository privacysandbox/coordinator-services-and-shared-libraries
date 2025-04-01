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
#ifndef CC_PBS_CONSUME_BUDGET_SRC_BINARY_BUDGET_CONSUMER_H_
#define CC_PBS_CONSUME_BUDGET_SRC_BINARY_BUDGET_CONSUMER_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/pbs/consume_budget/src/budget_consumer.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/pbs/proto/storage/budget_value.pb.h"
#include "cc/public/core/interface/execution_result.h"
#include "google/cloud/spanner/results.h"

namespace google::scp::pbs {

// A class that implements BudgetConsumer for numerical budgets to consume
// privacy budgets for a given list of privacy budget keys by reading from HTTP
// request body.
class BinaryBudgetConsumer : public BudgetConsumer {
 public:
  BinaryBudgetConsumer(
      privacy_sandbox::pbs_common::ConfigProviderInterface* config_provider)
      : config_provider_(config_provider) {}

  ~BinaryBudgetConsumer() override = default;

  google::scp::core::ExecutionResult ParseTransactionRequest(
      const privacy_sandbox::pbs_common::AuthContext& auth_context,
      const privacy_sandbox::pbs_common::HttpHeaders& request_headers,
      const nlohmann::json& request_body) override;

  google::cloud::spanner::KeySet GetSpannerKeySet() override;

  size_t GetKeyCount() override;

  SpannerMutationsResult ConsumeBudget(
      google::cloud::spanner::RowStream& row_stream,
      absl::string_view table_name) override;

  std::vector<std::string> DebugKeyList() override;

  google::scp::core::ExecutionResultOr<std::vector<std::string>>
  GetReadColumns() override;

 private:
  class PbsPrimaryKey {
   public:
    PbsPrimaryKey(std::string budget_key, std::string timeframe)
        : budget_key_(std::move(budget_key)),
          timeframe_(std::move(timeframe)) {}

    // Support hashing for absl::flat_hash_map
    // Hashing should be done only when class has valid entries for budget_key
    // and timeframe
    template <typename H>
    friend H AbslHashValue(H h, const PbsPrimaryKey& c) {
      return H::combine(std::move(h), c.budget_key_, c.timeframe_);
    }

    friend bool operator==(const PbsPrimaryKey& p1, const PbsPrimaryKey& p2) {
      return p1.budget_key_ == p2.budget_key_ && p1.timeframe_ == p2.timeframe_;
    }

    google::cloud::spanner::Key ToSpannerKey() const {
      return google::cloud::spanner::MakeKey(budget_key_, timeframe_);
    }

    operator std::string() const { return budget_key_ + "," + timeframe_; }

    const std::string& GetBudgetKey() const { return budget_key_; }

    const std::string& GetTimeframe() const { return timeframe_; }

   private:
    std::string budget_key_;  // First column of the database
    std::string timeframe_;   // Second column of the database
  };

  struct ConsumptionState {
    // Budget consumption requests for multiple hours of day for the same key
    absl::flat_hash_map<TimeBucket, size_t> hour_of_day_to_key_index_map;

    bool is_key_already_present_in_database = false;
    std::array<int8_t, /*token_count=*/24> budget_state{};
  };

  google::scp::core::ExecutionResult ParseRequestBodyV1(
      absl::string_view transaction_origin, const nlohmann::json& request_body);

  google::scp::core::ExecutionResult ParseRequestBodyV2(
      absl::string_view authorized_domain, const nlohmann::json& request_body);

  google::scp::core::ExecutionResult SetMigrationPhaseStates();

  template <typename TokenMetadataType>
    requires(
        std::is_same_v<TokenMetadataType, google::cloud::spanner::Json> ||
        std::is_same_v<TokenMetadataType, privacy_sandbox_pbs::BudgetValue>)
  SpannerMutationsResult MutateConsumptionStateForKeysPresentInDatabase(
      google::cloud::spanner::RowStream& row_stream);

  void MutateConsumptionStateForKeysNotPresentInDatabase();

  google::cloud::spanner::Mutations GenerateSpannerMutations(
      absl::string_view table_name);

  absl::flat_hash_map<PbsPrimaryKey, ConsumptionState> metadata_;
  privacy_sandbox::pbs_common::ConfigProviderInterface* config_provider_;
  size_t key_count_ = 0;

  bool enable_write_to_value_column_ = false;
  bool enable_write_to_value_proto_column_ = false;
  bool enable_read_truth_from_value_column_ = false;
};

}  // namespace google::scp::pbs

#endif  // CC_PBS_CONSUME_BUDGET_SRC_BINARY_BUDGET_CONSUMER_H_
