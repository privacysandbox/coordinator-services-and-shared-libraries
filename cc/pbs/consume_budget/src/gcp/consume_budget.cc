// Copyright 2024 Google LLC
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
#include "cc/pbs/consume_budget/src/gcp/consume_budget.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/pbs/budget_key_timeframe_manager/src/budget_key_timeframe_serialization.h"
#include "cc/pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/errors.h"
#include "cc/public/core/interface/execution_result.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mutations.h"

namespace google::scp::pbs {
namespace {

using ::google::scp::core::AsyncContext;
using ::google::scp::core::AsyncExecutorInterface;
using ::google::scp::core::ConfigProviderInterface;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::ExecutionResultOr;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::GetErrorMessage;
using ::google::scp::core::kGcpProjectId;
using ::google::scp::core::kSpannerDatabase;
using ::google::scp::core::kSpannerEndpointOverride;
using ::google::scp::core::kSpannerInstance;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::pbs::budget_key_timeframe_manager::Serialization;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_EXHAUSTED;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_FAIL_TO_COMMIT;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_INITIALIZATION_ERROR;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_PARSING_ERROR;
namespace spanner = ::google::cloud::spanner;

constexpr absl::string_view kComponentName = "BudgetConsumptionHelper";
constexpr absl::string_view kBudgetKeySpannerColumnName = "Budget_Key";
constexpr absl::string_view kTimeframeSpannerColumnName = "Timeframe";
constexpr absl::string_view kValueSpannerColumnName = "Value";
constexpr absl::string_view kTokenCountJsonField = "TokenCount";
constexpr size_t kDefaultTokenCountSize = 24;
constexpr TokenCount kDefaultPrivacyBudgetCount = 1;

class PbsPrimaryKey {
 public:
  PbsPrimaryKey(const std::string& budget_key, std::string timeframe)
      : budget_key_(budget_key), timeframe_(timeframe) {}

  const std::string& budget_key() const { return budget_key_; }

  const std::string& timeframe() const { return timeframe_; }

  template <typename H>
  friend H AbslHashValue(H h, const PbsPrimaryKey& c) {
    return H::combine(std::move(h), c.budget_key_, c.timeframe_);
  }

  friend bool operator==(const PbsPrimaryKey& p1, const PbsPrimaryKey& p2) {
    return p1.budget_key_ == p2.budget_key_ && p1.timeframe_ == p2.timeframe_;
  }

 private:
  std::string budget_key_;

  // Number of days since epoch. The type of this field is a std::string instead
  // of a TimeGroup or int to prevent expansive back and forth conversion
  // between int and string
  std::string timeframe_;
};

class PbsBudgetKeyMutation {
 public:
  void ResetTokenCount() {
    token_count_ = std::vector<TokenCount>(kDefaultTokenCountSize);
    token_count_.assign(kDefaultTokenCountSize, kDefaultPrivacyBudgetCount);
  }

  std::tuple<cloud::Status, ExecutionResult> ResetFromSpannerValue(
      const spanner::Json& spanner_json) {
    nlohmann::json json_value;
    try {
      json_value = nlohmann::json::parse(std::string(spanner_json));
    } catch (...) {
      return std::make_tuple(
          cloud::Status(cloud::StatusCode::kInvalidArgument,
                        "Failed to parse Value JSON column while reading "
                        "from BudgetKey table"),
          FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR));
    }

    if (!json_value.contains(std::string(kTokenCountJsonField))) {
      return std::make_tuple(
          cloud::Status(cloud::StatusCode::kInvalidArgument,
                        "The json in Value column does not contain TokenCount "
                        "json field"),
          FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR));
    }

    token_count_.clear();
    if (auto execution_result = Serialization::DeserializeHourTokensInTimeGroup(
            json_value[std::string(kTokenCountJsonField)], token_count_);
        !execution_result.Successful()) {
      return std::make_tuple(
          cloud::Status(
              cloud::StatusCode::kInvalidArgument,
              absl::StrCat(
                  "Unable to DeserializeHourTokensInTimeGroup. Json value: ",
                  std::string(json_value[std::string(kTokenCountJsonField)]))),
          FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR));
    }
    return std::make_tuple(cloud::Status(), SuccessExecutionResult());
  }

  std::tuple<cloud::Status, ExecutionResult, spanner::Json> ToSpannerJson()
      const {
    std::string serialized_token_count;
    if (auto execution_result = Serialization::SerializeHourTokensInTimeGroup(
            token_count_, serialized_token_count);
        !execution_result.Successful()) {
      return std::make_tuple(
          cloud::Status(
              cloud::StatusCode::kInvalidArgument,
              absl::StrCat(
                  "Unable to SerializeHourTokensInTimeGroup. message: ",
                  GetErrorMessage(execution_result.status_code))),
          FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR),
          spanner::Json());
    }

    nlohmann::json json_value;
    json_value[std::string(kTokenCountJsonField)] = serialized_token_count;
    return std::make_tuple(cloud::Status(), SuccessExecutionResult(),
                           spanner::Json(json_value.dump()));
  }

  int32_t GetTokenCount(size_t hour) const { return token_count_[hour]; }

  void SetTokenCount(size_t hour, int32_t count) { token_count_[hour] = count; }

  bool is_insertion() const { return is_insertion_; }

  void set_is_insertion(bool is_insertion) { is_insertion_ = is_insertion; }

 private:
  // Whether mutation is a Spanner insert mutation or Spanner update mutation
  bool is_insertion_ = false;

  // A vector with 24 integers, each represents an hour in a given day.
  std::vector<TokenCount> token_count_;
};

cloud::StatusOr<absl::flat_hash_map<PbsPrimaryKey, spanner::Json>>
ReadPrivacyBudgetsForKeys(cloud::spanner::Client client,
                          cloud::spanner::Transaction txn,
                          const std::string& table_name,
                          const cloud::spanner::KeySet& key_set) {
  spanner::RowStream returned_rows =
      client.Read(std::move(txn), table_name, std::move(key_set),
                  {std::string(kBudgetKeySpannerColumnName),
                   std::string(kTimeframeSpannerColumnName),
                   std::string(kValueSpannerColumnName)});
  absl::flat_hash_map<PbsPrimaryKey, spanner::Json> results;
  using RowType = std::tuple<std::string, std::string, spanner::Json>;
  for (const auto& row : cloud::spanner::StreamOf<RowType>(returned_rows)) {
    if (!row) {
      return row.status();
    }
    if (row.status().code() == cloud::StatusCode::kNotFound) {
      continue;
    }
    results.emplace(PbsPrimaryKey{std::get<0>(*row), std::get<1>(*row)},
                    std::get<2>(*row));
  }
  return results;
}

spanner::KeySet CreateSpannerKeySet(
    const std::vector<ConsumeBudgetMetadata>& budgets_metadata) {
  spanner::KeySet spanner_key_set;
  for (const ConsumeBudgetMetadata& metadata : budgets_metadata) {
    spanner_key_set.AddKey(spanner::MakeKey(
        *metadata.budget_key_name,
        absl::StrCat(budget_key_timeframe_manager::Utils::GetTimeGroup(
            metadata.time_bucket))));
  }
  return spanner_key_set;
}

std::tuple<cloud::Status, ExecutionResult> CreatePbsMutations(
    const absl::flat_hash_map<PbsPrimaryKey, spanner::Json>& query_results,
    absl::flat_hash_map<PbsPrimaryKey, PbsBudgetKeyMutation>& pbs_mutations) {
  pbs_mutations.clear();
  for (const auto& [pbs_primary_key, spanner_json] : query_results) {
    PbsBudgetKeyMutation& pbs_mutation = pbs_mutations[pbs_primary_key];
    auto [status, execution_result] =
        pbs_mutation.ResetFromSpannerValue(spanner_json);
    if (!status.ok()) {
      return std::make_tuple(status, execution_result);
    }
  }
  return std::make_tuple(cloud::Status(), SuccessExecutionResult());
}

std::tuple<cloud::Status, ExecutionResult, std::vector<size_t>>
ValidatePbsMutations(
    const std::vector<ConsumeBudgetMetadata>& budgets_metadata,
    absl::flat_hash_map<PbsPrimaryKey, PbsBudgetKeyMutation>& pbs_mutations) {
  std::vector<size_t> budget_exhausted_indices;
  for (int i = 0; i < budgets_metadata.size(); ++i) {
    const ConsumeBudgetMetadata& metadata = budgets_metadata[i];
    PbsPrimaryKey primary_key{
        *metadata.budget_key_name,
        absl::StrCat(budget_key_timeframe_manager::Utils::GetTimeGroup(
            metadata.time_bucket))};
    auto pbs_mutation = pbs_mutations.find(primary_key);

    // The privacy budget key is seen for the first time in the database, so the
    // key must have enough budget
    if (pbs_mutation == pbs_mutations.end()) {
      continue;
    }

    TimeBucket hours_of_the_day =
        budget_key_timeframe_manager::Utils::GetTimeBucket(
            metadata.time_bucket);
    TokenCount current_token_count =
        pbs_mutation->second.GetTokenCount(hours_of_the_day);
    if (current_token_count < metadata.token_count) {
      budget_exhausted_indices.push_back(i);
    }
  }

  if (!budget_exhausted_indices.empty()) {
    return std::make_tuple(cloud::Status(cloud::StatusCode::kInvalidArgument,
                                         "Not enough budget."),
                           FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED),
                           budget_exhausted_indices);
  }
  return std::make_tuple(cloud::Status(), SuccessExecutionResult(),
                         budget_exhausted_indices);
}

std::tuple<cloud::Status, ExecutionResult> UpdatePbsMutationsToConsumeBudgets(
    const std::vector<ConsumeBudgetMetadata>& budgets_metadata,
    absl::flat_hash_map<PbsPrimaryKey, PbsBudgetKeyMutation>& pbs_mutations) {
  for (const ConsumeBudgetMetadata& metadata : budgets_metadata) {
    // GetTimeGroup returns the number of days since epoch
    PbsPrimaryKey primary_key{
        *metadata.budget_key_name,
        absl::StrCat(budget_key_timeframe_manager::Utils::GetTimeGroup(
            metadata.time_bucket))};
    auto pbs_mutation = pbs_mutations.find(primary_key);
    if (pbs_mutation == pbs_mutations.end()) {
      PbsBudgetKeyMutation& m = pbs_mutations[primary_key];
      m.ResetTokenCount();
      m.set_is_insertion(true);
      pbs_mutation = pbs_mutations.find(primary_key);
    }

    TimeBucket hours_of_the_day =
        budget_key_timeframe_manager::Utils::GetTimeBucket(
            metadata.time_bucket);
    TokenCount current_token_count =
        pbs_mutation->second.GetTokenCount(hours_of_the_day);
    if (current_token_count < metadata.token_count) {
      return std::make_tuple(
          cloud::Status(cloud::StatusCode::kInvalidArgument,
                        absl::StrCat("Not enough budget. BudgetKey: ",
                                     primary_key.budget_key(),
                                     "; Timeframe: ", primary_key.timeframe())),
          FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED));
    }

    pbs_mutation->second.SetTokenCount(
        hours_of_the_day, current_token_count - metadata.token_count);
  }
  return std::make_tuple(cloud::Status(), SuccessExecutionResult());
}

std::tuple<cloud::Status, ExecutionResult> CreateSpannerMutations(
    const absl::flat_hash_map<PbsPrimaryKey, PbsBudgetKeyMutation>&
        pbs_mutations,
    absl::string_view table_name, spanner::Mutations& mutations) {
  auto insertion_builder = spanner::InsertMutationBuilder(
      std::string(table_name), {std::string(kBudgetKeySpannerColumnName),
                                std::string(kTimeframeSpannerColumnName),
                                std::string(kValueSpannerColumnName)});
  bool has_insert = false;
  auto update_builder = spanner::UpdateMutationBuilder(
      std::string(table_name), {std::string(kBudgetKeySpannerColumnName),
                                std::string(kTimeframeSpannerColumnName),
                                std::string(kValueSpannerColumnName)});
  bool has_update = false;
  for (const auto& [pbs_key, pbs_mutation] : pbs_mutations) {
    auto [status, execution_result, json] = pbs_mutation.ToSpannerJson();
    if (!status.ok()) {
      return std::make_tuple(status, execution_result);
    }
    if (pbs_mutation.is_insertion()) {
      insertion_builder.EmplaceRow(pbs_key.budget_key(), pbs_key.timeframe(),
                                   json);
      has_insert = true;
    } else {
      update_builder.EmplaceRow(pbs_key.budget_key(), pbs_key.timeframe(),
                                json);
      has_update = true;
    }
  }

  mutations.clear();
  if (has_insert) {
    mutations.push_back(insertion_builder.Build());
  }
  if (has_update) {
    mutations.push_back(update_builder.Build());
  }
  return std::make_tuple(cloud::Status(), SuccessExecutionResult());
}
}  // namespace

BudgetConsumptionHelper::BudgetConsumptionHelper(
    ConfigProviderInterface* config_provider,
    AsyncExecutorInterface* async_executor,
    AsyncExecutorInterface* io_async_executor,
    std::shared_ptr<spanner::Connection> spanner_connection)
    : config_provider_(config_provider),
      async_executor_(async_executor),
      io_async_executor_(io_async_executor),
      spanner_connection_(std::move(spanner_connection)) {}

ExecutionResultOr<std::shared_ptr<cloud::spanner::Connection>>
BudgetConsumptionHelper::MakeSpannerConnectionForProd(
    ConfigProviderInterface& config_provider) {
  std::string project;
  if (auto execution_result = config_provider.Get(kGcpProjectId, project);
      !execution_result.Successful()) {
    return execution_result;
  }

  std::string instance;
  if (auto execution_result = config_provider.Get(kSpannerInstance, instance);
      !execution_result.Successful()) {
    return execution_result;
  }

  std::string database;
  if (auto execution_result = config_provider.Get(kSpannerDatabase, database);
      !execution_result.Successful()) {
    return execution_result;
  }

  google::cloud::Options options;

  std::string endpoint_override;
  if (auto execution_result =
          config_provider.Get(kSpannerEndpointOverride, endpoint_override);
      execution_result.Successful()) {
    options.set<google::cloud::EndpointOption>(endpoint_override);
  }

  return spanner::MakeConnection(spanner::Database(project, instance, database),
                                 options);
}

ExecutionResult BudgetConsumptionHelper::Init() noexcept {
  if (!spanner_connection_) {
    return FailureExecutionResult(SC_CONSUME_BUDGET_INITIALIZATION_ERROR);
  }
  if (auto execution_result =
          config_provider_->Get(kBudgetKeyTableName, table_name_);
      execution_result != SuccessExecutionResult()) {
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult BudgetConsumptionHelper::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult BudgetConsumptionHelper::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult BudgetConsumptionHelper::ConsumeBudgets(
    AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
        consume_budgets_context) {
  // TODO: Check that request is not empty.
  // Return invalid argument
  if (auto schedule_result = io_async_executor_->Schedule(
          [this, consume_budgets_context]() {
            ConsumeBudgetsSyncAndFinishContext(consume_budgets_context);
          },
          google::scp::core::AsyncPriority::Normal);
      !schedule_result.Successful()) {
    // Returns the execution result to the caller without calling FinishContext,
    // since the async task is not scheduled successfully
    return schedule_result;
  }
  return SuccessExecutionResult();
}

void BudgetConsumptionHelper::ConsumeBudgetsSyncAndFinishContext(
    AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
        consume_budgets_context) {
  consume_budgets_context.result = ConsumeBudgetsSync(consume_budgets_context);
  if (!async_executor_->Schedule(
          [consume_budgets_context]() mutable {
            consume_budgets_context.Finish();
          },
          google::scp::core::AsyncPriority::Normal)) {
    consume_budgets_context.Finish();
  }
}

ExecutionResult BudgetConsumptionHelper::ConsumeBudgetsSync(
    const AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>&
        consume_budgets_context) {
  spanner::Client client(spanner_connection_);
  ExecutionResult captured_execution_result = SuccessExecutionResult();
  std::vector<size_t> captured_budget_exhausted_indices;
  auto commit_result = client.Commit(
      [&](spanner::Transaction txn) -> cloud::StatusOr<spanner::Mutations> {
        spanner::KeySet spanner_key_set =
            CreateSpannerKeySet(consume_budgets_context.request->budgets);
        cloud::StatusOr<absl::flat_hash_map<PbsPrimaryKey, spanner::Json>>
            results = ReadPrivacyBudgetsForKeys(client, txn, table_name_,
                                                spanner_key_set);
        if (!results.ok()) {
          return results.status();
        }

        absl::flat_hash_map<PbsPrimaryKey, PbsBudgetKeyMutation> pbs_mutations;
        if (auto [status, execution_result] =
                CreatePbsMutations(*results, pbs_mutations);
            !status.ok()) {
          captured_execution_result = execution_result;
          return status;
        }

        if (auto [status, execution_result, budget_exhausted_indices] =
                ValidatePbsMutations(consume_budgets_context.request->budgets,
                                     pbs_mutations);
            !status.ok()) {
          captured_execution_result = execution_result;
          captured_budget_exhausted_indices = budget_exhausted_indices;
          return status;
        }

        if (auto [status, execution_result] =
                UpdatePbsMutationsToConsumeBudgets(
                    consume_budgets_context.request->budgets, pbs_mutations);
            !status.ok()) {
          captured_execution_result = execution_result;
          return status;
        }

        spanner::Mutations mutations;
        if (auto [status, execution_result] =
                CreateSpannerMutations(pbs_mutations, table_name_, mutations);
            !status.ok()) {
          captured_execution_result = execution_result;
          return status;
        }
        return mutations;
      });

  if (!commit_result) {
    if (captured_execution_result.status_code == SC_CONSUME_BUDGET_EXHAUSTED) {
      consume_budgets_context.response->budget_exhausted_indices =
          captured_budget_exhausted_indices;
    }

    // If the error status is coming from PBS's application logics, the
    // captured_execution_result should contain failure result. Otherwise, the
    // error status is considered to be coming from Spanner. In this case, the
    // spanner error codes will be transformed to a failed ExecutionResult with
    // INTERNAL SERVER ERROR.
    auto final_execution_result =
        !captured_execution_result.Successful()
            ? captured_execution_result
            : FailureExecutionResult(SC_CONSUME_BUDGET_FAIL_TO_COMMIT);
    if (captured_execution_result.status_code == SC_CONSUME_BUDGET_EXHAUSTED) {
      SCP_WARNING_CONTEXT(
          kComponentName, consume_budgets_context,
          absl::StrFormat("ConsumeBudgets failed. Error code %d, message: %s, "
                          "final_execution_result: %s",
                          commit_result.status().code(),
                          commit_result.status().message(),
                          google::scp::core::errors::GetErrorMessage(
                              final_execution_result.status_code)));
    } else {
      SCP_ERROR_CONTEXT(
          kComponentName, consume_budgets_context, final_execution_result,
          absl::StrFormat("ConsumeBudgets failed. Error code %d, message: %s",
                          commit_result.status().code(),
                          commit_result.status().message()));
    }
    return final_execution_result;
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::pbs
