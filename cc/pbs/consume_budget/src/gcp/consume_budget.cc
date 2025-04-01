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
#include <type_traits>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/pbs/budget_key_timeframe_manager/src/budget_key_timeframe_serialization.h"
#include "cc/pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"
#include "cc/pbs/consume_budget/src/budget_consumer.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/pbs/proto/storage/budget_value.pb.h"
#include "cc/public/core/interface/errors.h"
#include "cc/public/core/interface/execution_result.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mutations.h"

namespace google::scp::pbs {
namespace {

using ::google::scp::core::ExecutionResult;
using ::google::scp::core::ExecutionResultOr;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::pbs::budget_key_timeframe_manager::Serialization;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_EXHAUSTED;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_FAIL_TO_COMMIT;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_INITIALIZATION_ERROR;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_PARSING_ERROR;
using ::privacy_sandbox::pbs_common::AsyncContext;
using ::privacy_sandbox::pbs_common::AsyncExecutorInterface;
using ::privacy_sandbox::pbs_common::AsyncPriority;
using ::privacy_sandbox::pbs_common::ConfigProviderInterface;
using ::privacy_sandbox::pbs_common::GetErrorMessage;
using ::privacy_sandbox::pbs_common::kGcpProjectId;
using ::privacy_sandbox::pbs_common::kSpannerDatabase;
using ::privacy_sandbox::pbs_common::kSpannerEndpointOverride;
using ::privacy_sandbox::pbs_common::kSpannerInstance;
namespace spanner = ::google::cloud::spanner;

constexpr absl::string_view kComponentName = "BudgetConsumptionHelper";
constexpr absl::string_view kBudgetKeySpannerColumnName = "Budget_Key";
constexpr absl::string_view kTimeframeSpannerColumnName = "Timeframe";
constexpr absl::string_view kValueSpannerColumnName = "Value";
constexpr absl::string_view kValueProtoSpannerColumnName = "ValueProto";
constexpr absl::string_view kTokenCountJsonField = "TokenCount";
constexpr size_t kDefaultTokenCountSize = 24;
constexpr TokenCount kDefaultPrivacyBudgetCount = 1;
constexpr int32_t kDefaultLaplaceDpBudgetCount = 6400;
constexpr int32_t kEmptyBudgetCount = 0;

// Migration phase for ValueProto column.
// The new ValueProto column is meant to replace the existing Value JSON column.
// The data from Value JSON column needs to be migrated to ValueProto column.
// The migration is divided into four phases:
//
// - Phase 1:
//   - Value column is the source of truth (i.e. budget values will be read from
//     Value column)
//   - Budgets will be written to Value column
// - Phase 2:
//   - Value column is the source of truth (i.e. budget values will be read from
//     Value column)
//   - Budgets will be written to Value and ValueProto column
// - Phase 3:
//   - ValueProto column is the source of truth (i.e. budget values will be read
//   from ValueProto column)
//   - Budgets will be written to Value and ValueProto column
// - Phase 4:
//   - ValueProto column is the source of truth
//   - Budgets will be written to ValueProto column
//   - Value Column isn't read or written anymore.
constexpr absl::string_view kMigrationPhase1 = "phase_1";
constexpr absl::string_view kMigrationPhase2 = "phase_2";
constexpr absl::string_view kMigrationPhase3 = "phase_3";
constexpr absl::string_view kMigrationPhase4 = "phase_4";
constexpr std::array<absl::string_view, 4> kMigrationPhases = {
    kMigrationPhase1, kMigrationPhase2, kMigrationPhase3, kMigrationPhase4};

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

  std::tuple<cloud::Status, ExecutionResult> ResetFromSpannerValue(
      const privacy_sandbox_pbs::BudgetValue& spanner_value) {
    if (!spanner_value.has_laplace_dp_budgets()) {
      return std::make_tuple(
          cloud::Status(cloud::StatusCode::kInvalidArgument,
                        "Proto does not have LaplaceDpBudgets"),
          FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR));
    }

    const privacy_sandbox_pbs::BudgetValue::LaplaceDpBudgets& dp_budgets =
        spanner_value.laplace_dp_budgets();
    if (dp_budgets.budgets_size() != kDefaultTokenCountSize) {
      return std::make_tuple(
          cloud::Status(
              cloud::StatusCode::kInvalidArgument,
              absl::StrFormat(
                  "LaplaceDpBudgets have %d tokens, expected %d tokens",
                  dp_budgets.budgets_size(), kDefaultTokenCountSize)),
          FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR));
    }

    token_count_.clear();
    token_count_.resize(kDefaultTokenCountSize);
    for (size_t i = 0; i < kDefaultTokenCountSize; ++i) {
      const int32_t budget = dp_budgets.budgets(i);
      if (budget != kEmptyBudgetCount &&
          budget != kDefaultLaplaceDpBudgetCount) {
        return std::make_tuple(
            cloud::Status(
                cloud::StatusCode::kInvalidArgument,
                absl::StrFormat("LaplaceDpBudgets value should be "
                                "either %d (full) or %d (empty), found %d",
                                kDefaultLaplaceDpBudgetCount, kEmptyBudgetCount,
                                budget)),
            FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR));
      }
      token_count_[i] = budget == kDefaultLaplaceDpBudgetCount
                            ? kDefaultPrivacyBudgetCount
                            : kEmptyBudgetCount;
    }
    return std::make_tuple(cloud::Status(), SuccessExecutionResult());
  }

  std::tuple<cloud::Status, ExecutionResult, privacy_sandbox_pbs::BudgetValue>
  ToBudgetValue() const {
    privacy_sandbox_pbs::BudgetValue budget_value;
    budget_value.mutable_laplace_dp_budgets()->mutable_budgets()->Reserve(
        kDefaultTokenCountSize);

    privacy_sandbox_pbs::BudgetValue::LaplaceDpBudgets* dp_budgets =
        budget_value.mutable_laplace_dp_budgets();
    for (const auto& token_count : token_count_) {
      dp_budgets->add_budgets(token_count == kDefaultPrivacyBudgetCount
                                  ? kDefaultLaplaceDpBudgetCount
                                  : kEmptyBudgetCount);
    }
    return std::make_tuple(cloud::Status(), SuccessExecutionResult(),
                           budget_value);
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

  bool IsInsertion() const { return is_insertion_; }

  void SetIsInsertion(bool is_insertion) { is_insertion_ = is_insertion; }

 private:
  // Whether mutation is a Spanner insert mutation or Spanner update mutation
  bool is_insertion_ = false;

  // A vector with 24 integers, each represents an hour in a given day.
  std::vector<TokenCount> token_count_;
};

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

template <typename TokenMetadataType>
  requires(std::is_same_v<TokenMetadataType, spanner::Json> ||
           std::is_same_v<TokenMetadataType, privacy_sandbox_pbs::BudgetValue>)
std::tuple<cloud::Status, ExecutionResult> ReadPrivacyBudgetsForKeys(
    cloud::spanner::Client client, cloud::spanner::Transaction txn,
    const std::string& table_name, const cloud::spanner::KeySet& key_set,
    absl::flat_hash_map<PbsPrimaryKey, PbsBudgetKeyMutation>& pbs_mutations) {
  std::vector<std::string> columns = {std::string(kBudgetKeySpannerColumnName),
                                      std::string(kTimeframeSpannerColumnName)};

  using TokenMetadataTypeBaseT = std::decay_t<TokenMetadataType>;
  if constexpr (std::is_same_v<TokenMetadataTypeBaseT, spanner::Json>) {
    columns.emplace_back(kValueSpannerColumnName);
  } else {
    columns.emplace_back(kValueProtoSpannerColumnName);
  }

  using ValueColumnType = std::conditional_t<
      std::is_same_v<TokenMetadataTypeBaseT, spanner::Json>, spanner::Json,
      spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>>;
  using RowType = std::tuple<std::string, std::string, ValueColumnType>;

  spanner::RowStream returned_rows =
      client.Read(std::move(txn), table_name, std::move(key_set), columns);

  for (const auto& row : cloud::spanner::StreamOf<RowType>(returned_rows)) {
    if (!row) {
      return std::make_tuple(
          cloud::Status(cloud::StatusCode::kInvalidArgument,
                        absl::StrFormat(
                            "Error reading rows from the database. Reason: %s",
                            row.status().message())),
          FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR));
    }
    if (row.status().code() == cloud::StatusCode::kNotFound) {
      continue;
    }

    PbsBudgetKeyMutation& pbs_mutation =
        pbs_mutations[PbsPrimaryKey{std::get<0>(*row), std::get<1>(*row)}];

    std::tuple<cloud::Status, ExecutionResult> result;
    if constexpr (std::is_same_v<TokenMetadataTypeBaseT, spanner::Json>) {
      result = pbs_mutation.ResetFromSpannerValue(std::get<2>(*row));
    } else {
      result = pbs_mutation.ResetFromSpannerValue(
          privacy_sandbox_pbs::BudgetValue(std::get<2>(*row)));
    }
    if (!std::get<1>(result)) {
      return result;
    }
  }
  return std::make_tuple(cloud::Status(), SuccessExecutionResult());
}

std::tuple<cloud::Status, ExecutionResult, std::vector<size_t>>
UpdatePbsMutationsToConsumeBudgetsOrNotifyBudgetExhausted(
    const std::vector<ConsumeBudgetMetadata>& budgets_metadata,
    absl::flat_hash_map<PbsPrimaryKey, PbsBudgetKeyMutation>& pbs_mutations) {
  std::vector<size_t> budget_exhausted_indices;
  for (size_t i = 0; i < budgets_metadata.size(); ++i) {
    const ConsumeBudgetMetadata& metadata = budgets_metadata[i];
    // GetTimeGroup returns the number of days since epoch
    PbsPrimaryKey primary_key{
        *metadata.budget_key_name,
        absl::StrCat(budget_key_timeframe_manager::Utils::GetTimeGroup(
            metadata.time_bucket))};

    auto [pbs_mutation, inserted] =
        pbs_mutations.insert({primary_key, PbsBudgetKeyMutation()});
    if (inserted) {
      pbs_mutation->second.ResetTokenCount();
      pbs_mutation->second.SetIsInsertion(true);
    }

    TimeBucket hours_of_the_day =
        budget_key_timeframe_manager::Utils::GetTimeBucket(
            metadata.time_bucket);
    TokenCount current_token_count =
        pbs_mutation->second.GetTokenCount(hours_of_the_day);

    if (current_token_count < metadata.token_count) {
      // Do not process if there is no enough budget
      budget_exhausted_indices.push_back(i);
      continue;
    }

    pbs_mutation->second.SetTokenCount(
        hours_of_the_day, current_token_count - metadata.token_count);
  }

  if (!budget_exhausted_indices.empty()) {
    pbs_mutations.clear();
    return std::make_tuple(cloud::Status(cloud::StatusCode::kInvalidArgument,
                                         "Not enough budget."),
                           FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED),
                           budget_exhausted_indices);
  }
  return std::make_tuple(cloud::Status(), SuccessExecutionResult(),
                         budget_exhausted_indices);
}

std::tuple<cloud::Status, ExecutionResult> CreateSpannerMutations(
    const absl::flat_hash_map<PbsPrimaryKey, PbsBudgetKeyMutation>&
        pbs_mutations,
    absl::string_view table_name, bool enable_write_to_value_column,
    bool enable_write_to_value_proto_column, spanner::Mutations& mutations) {
  std::vector<std::string> columns = {
      std::string(kBudgetKeySpannerColumnName),
      std::string(kTimeframeSpannerColumnName),
  };
  if (enable_write_to_value_column) {
    columns.emplace_back(kValueSpannerColumnName);
  }
  if (enable_write_to_value_proto_column) {
    columns.emplace_back(kValueProtoSpannerColumnName);
  }

  auto insertion_builder =
      spanner::InsertMutationBuilder(std::string(table_name), columns);
  bool has_insert = false;
  auto update_builder =
      spanner::UpdateMutationBuilder(std::string(table_name), columns);
  bool has_update = false;

  for (const auto& [pbs_key, pbs_mutation] : pbs_mutations) {
    std::vector<spanner::Value> values = {spanner::Value(pbs_key.budget_key()),
                                          spanner::Value(pbs_key.timeframe())};

    if (enable_write_to_value_column) {
      auto [status, execution_result, json] = pbs_mutation.ToSpannerJson();
      if (!status.ok()) {
        return std::make_tuple(status, execution_result);
      }
      values.emplace_back(json);
    }

    if (enable_write_to_value_proto_column) {
      auto [status, execution_result, proto_val] = pbs_mutation.ToBudgetValue();
      values.emplace_back(
          spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>(proto_val));
    }

    if (pbs_mutation.IsInsertion()) {
      insertion_builder.AddRow(values);
      has_insert = true;
    } else {
      update_builder.AddRow(values);
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
      spanner_connection_(std::move(spanner_connection)),
      should_enable_budget_consumer_(false) {}

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

  std::string pbs_value_column_migration_phase = std::string(kMigrationPhase1);
  config_provider_->Get(kValueProtoMigrationPhase,
                        pbs_value_column_migration_phase);
  if (!absl::c_any_of(kMigrationPhases, [&pbs_value_column_migration_phase](
                                            absl::string_view valid_phase) {
        return pbs_value_column_migration_phase == valid_phase;
      })) {
    return FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR);
  }

  enable_write_to_value_column_ =
      pbs_value_column_migration_phase == kMigrationPhase1 ||
      pbs_value_column_migration_phase == kMigrationPhase2 ||
      pbs_value_column_migration_phase == kMigrationPhase3;
  enable_write_to_value_proto_column_ =
      pbs_value_column_migration_phase == kMigrationPhase2 ||
      pbs_value_column_migration_phase == kMigrationPhase3 ||
      pbs_value_column_migration_phase == kMigrationPhase4;
  enable_read_truth_from_value_column_ =
      pbs_value_column_migration_phase == kMigrationPhase1 ||
      pbs_value_column_migration_phase == kMigrationPhase2;

  config_provider_->Get(kEnableBudgetConsumerMigration,
                        should_enable_budget_consumer_);

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
          AsyncPriority::Normal);
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
  if (should_enable_budget_consumer_) {
    consume_budgets_context.result =
        ConsumeBudgetsSyncWithBudgetConsumer(consume_budgets_context);
  } else {
    consume_budgets_context.result =
        ConsumeBudgetsSyncWithoutBudgetConsumer(consume_budgets_context);
  }

  if (!async_executor_->Schedule(
          [consume_budgets_context]() mutable {
            consume_budgets_context.Finish();
          },
          AsyncPriority::Normal)) {
    consume_budgets_context.Finish();
  }
}

ExecutionResult
BudgetConsumptionHelper::ConsumeBudgetsSyncWithoutBudgetConsumer(
    const AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>&
        consume_budgets_context) {
  spanner::Client client(spanner_connection_);
  ExecutionResult captured_execution_result = SuccessExecutionResult();
  std::vector<size_t> captured_budget_exhausted_indices;
  auto commit_result = client.Commit(
      [&](spanner::Transaction txn) -> cloud::StatusOr<spanner::Mutations> {
        spanner::KeySet spanner_key_set =
            CreateSpannerKeySet(consume_budgets_context.request->budgets);

        absl::flat_hash_map<PbsPrimaryKey, PbsBudgetKeyMutation> pbs_mutations;

        if (enable_read_truth_from_value_column_) {
          if (auto [status, execution_result] =
                  ReadPrivacyBudgetsForKeys<spanner::Json>(
                      client, txn, table_name_, spanner_key_set, pbs_mutations);
              !status.ok()) {
            captured_execution_result = execution_result;
            return status;
          }
        } else {
          if (auto [status, execution_result] =
                  ReadPrivacyBudgetsForKeys<privacy_sandbox_pbs::BudgetValue>(
                      client, txn, table_name_, spanner_key_set, pbs_mutations);
              !status.ok()) {
            captured_execution_result = execution_result;
            return status;
          }
        }

        if (auto [status, execution_result, budget_exhausted_indices] =
                UpdatePbsMutationsToConsumeBudgetsOrNotifyBudgetExhausted(
                    consume_budgets_context.request->budgets, pbs_mutations);
            !status.ok()) {
          captured_execution_result = execution_result;
          captured_budget_exhausted_indices = budget_exhausted_indices;
          return status;
        }

        spanner::Mutations mutations;
        if (auto [status, execution_result] = CreateSpannerMutations(
                pbs_mutations, table_name_, enable_write_to_value_column_,
                enable_write_to_value_proto_column_, mutations);
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
                          privacy_sandbox::pbs_common::GetErrorMessage(
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

ExecutionResult BudgetConsumptionHelper::ConsumeBudgetsSyncWithBudgetConsumer(
    const AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>&
        consume_budgets_context) {
  spanner::Client client(spanner_connection_);
  ExecutionResult captured_execution_result = SuccessExecutionResult();

  auto commit_result = client.Commit(
      [&](spanner::Transaction txn) -> cloud::StatusOr<spanner::Mutations> {
        BudgetConsumer& budget_consumer =
            *consume_budgets_context.request->budget_consumer;
        spanner::KeySet spanner_key_set = budget_consumer.GetSpannerKeySet();

        auto columns = budget_consumer.GetReadColumns();
        if (!columns.Successful()) {
          captured_execution_result =
              FailureExecutionResult(SC_CONSUME_BUDGET_INITIALIZATION_ERROR);
          return cloud::Status(cloud::StatusCode::kInvalidArgument,
                               "Cannot fetch the columns to read");
        }

        spanner::RowStream row_stream =
            client.Read(txn, table_name_, std::move(spanner_key_set), *columns);
        SpannerMutationsResult spanner_mutations_result =
            budget_consumer.ConsumeBudget(row_stream, table_name_);

        consume_budgets_context.response->budget_exhausted_indices =
            spanner_mutations_result.budget_exhausted_indices;
        captured_execution_result = spanner_mutations_result.execution_result;
        if (spanner_mutations_result.status.ok()) {
          return spanner_mutations_result.mutations;
        }
        return spanner_mutations_result.status;
      });

  if (!commit_result) {
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
                          privacy_sandbox::pbs_common::GetErrorMessage(
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
