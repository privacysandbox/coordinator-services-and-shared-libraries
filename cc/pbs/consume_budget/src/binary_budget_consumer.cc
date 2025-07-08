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

#include "cc/pbs/consume_budget/src/binary_budget_consumer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/http_types.h"
#include "cc/pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"
#include "cc/pbs/budget_key_timeframe_manager/src/error_codes.h"
#include "cc/pbs/consume_budget/src/budget_consumer.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/front_end_service/src/front_end_utils.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs {

namespace {

using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest;
using ::privacy_sandbox::pbs_common::AuthContext;
using ::privacy_sandbox::pbs_common::ConfigProviderInterface;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::ExecutionResultOr;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::kZeroUuid;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
namespace spanner = google::cloud::spanner;

constexpr absl::string_view kBinaryBudgetConsumer = "BinaryBudgetConsumer";
constexpr absl::string_view kTokenCountJsonField = "TokenCount";
constexpr int32_t kDefaultLaplaceDpBudgetCount = 6400;
constexpr int8_t kFullBudgetCount = 1;
constexpr int8_t kEmptyBudgetCount = 0;
constexpr size_t kDefaultTokenCountSize = 24;

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

ExecutionResultOr<TimeBucket> ReportingTimeToTimeBucket(
    const std::string& reporting_time) {
  google::protobuf::Timestamp reporting_timestamp;
  if (!google::protobuf::util::TimeUtil::FromString(reporting_time,
                                                    &reporting_timestamp)) {
    return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
  }

  if (reporting_timestamp.seconds() < 0) {
    return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
  }

  uint64_t seconds_since_epoch = reporting_timestamp.seconds();
  std::chrono::nanoseconds reporting_time_nanoseconds =
      std::chrono::seconds(seconds_since_epoch);
  return static_cast<uint64_t>(reporting_time_nanoseconds.count());
}

std::tuple<google::cloud::Status, ExecutionResult> VerifyLaplaceProto(
    const privacy_sandbox_pbs::BudgetValue& spanner_value) {
  if (!spanner_value.has_laplace_dp_budgets()) {
    return std::make_tuple(
        google::cloud::Status(google::cloud::StatusCode::kInvalidArgument,
                              "Proto does not have LaplaceDpBudgets"),
        FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR));
  }

  const privacy_sandbox_pbs::BudgetValue::LaplaceDpBudgets& dp_budgets =
      spanner_value.laplace_dp_budgets();
  if (dp_budgets.budgets_size() != kDefaultTokenCountSize) {
    return std::make_tuple(
        google::cloud::Status(
            google::cloud::StatusCode::kInvalidArgument,
            absl::StrFormat(
                "LaplaceDpBudgets have %d tokens, expected %d tokens",
                dp_budgets.budgets_size(), kDefaultTokenCountSize)),
        FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR));
  }

  return std::make_tuple(google::cloud::Status(), SuccessExecutionResult());
}

ExecutionResult DeserializeHourTokensInTimeGroup(
    const std::string& hour_token_in_time_group,
    std::array<int8_t, kDefaultTokenCountSize>& hour_tokens) {
  std::vector<absl::string_view> tokens_per_hour =
      absl::StrSplit(hour_token_in_time_group, " ");

  if (tokens_per_hour.size() != kDefaultTokenCountSize) {
    return FailureExecutionResult(
        SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
  }

  for (int i = 0; i < kDefaultTokenCountSize; ++i) {
    int32_t value;
    if (!absl::SimpleAtoi(tokens_per_hour[i], &value) ||
        value < kEmptyBudgetCount || value > kFullBudgetCount) {
      return FailureExecutionResult(
          SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
    }
    hour_tokens[i] = static_cast<int8_t>(value);
  }

  return SuccessExecutionResult();
}

std::tuple<google::cloud::Status, ExecutionResult,
           std::array<int8_t, kDefaultTokenCountSize>>
ParseSpannerJson(const spanner::Json& spanner_json) {
  std::array<int8_t, kDefaultTokenCountSize> result;

  nlohmann::json json_value;
  try {
    json_value = nlohmann::json::parse(std::string(spanner_json));
  } catch (...) {
    return std::make_tuple(
        google::cloud::Status(google::cloud::StatusCode::kInvalidArgument,
                              "Failed to parse Value JSON column while reading "
                              "from BudgetKey table"),
        FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR), result);
  }

  if (!json_value.contains(std::string(kTokenCountJsonField))) {
    return std::make_tuple(
        google::cloud::Status(
            google::cloud::StatusCode::kInvalidArgument,
            "The json in Value column does not contain TokenCount "
            "json field"),
        FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR), result);
  }

  if (auto execution_result = DeserializeHourTokensInTimeGroup(
          json_value[std::string(kTokenCountJsonField)], result);
      !execution_result.Successful()) {
    return std::make_tuple(
        google::cloud::Status(
            google::cloud::StatusCode::kInvalidArgument,
            absl::StrCat(
                "Unable to DeserializeHourTokensInTimeGroup. Json value: ",
                std::string(json_value[std::string(kTokenCountJsonField)]))),
        FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR), result);
  }
  return std::make_tuple(google::cloud::Status(), SuccessExecutionResult(),
                         result);
}

spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue> CreateLaplaceProto(
    const std::array<int8_t, kDefaultTokenCountSize>& budgets) {
  privacy_sandbox_pbs::BudgetValue budget_value;
  budget_value.mutable_laplace_dp_budgets()->mutable_budgets()->Reserve(
      kDefaultTokenCountSize);

  privacy_sandbox_pbs::BudgetValue::LaplaceDpBudgets* dp_budgets =
      budget_value.mutable_laplace_dp_budgets();
  for (const auto& token_count : budgets) {
    dp_budgets->add_budgets(token_count == kFullBudgetCount
                                ? kDefaultLaplaceDpBudgetCount
                                : kEmptyBudgetCount);
  }
  return budget_value;
}

std::string SerializeHourTokensInTimeGroup(
    const std::array<int8_t, kDefaultTokenCountSize>& hour_tokens) {
  return absl::StrJoin(hour_tokens, " ", [](std::string* out, int token) {
    absl::StrAppend(out, std::to_string(token));
  });
}

spanner::Json CreateSpannerJson(
    const std::array<int8_t, kDefaultTokenCountSize>& budgets) {
  const std::string serialized_token_count =
      SerializeHourTokensInTimeGroup(budgets);

  nlohmann::json json_value;
  json_value[std::string(kTokenCountJsonField)] = serialized_token_count;
  return spanner::Json(json_value.dump());
}
}  // namespace

BinaryBudgetConsumer::BinaryBudgetConsumer(
    ConfigProviderInterface* config_provider)
    : config_provider_(config_provider) {
  std::string pbs_value_column_migration_phase;
  ExecutionResult execution_result = config_provider_->Get(
      kValueProtoMigrationPhase, pbs_value_column_migration_phase);
  if (!execution_result.Successful() ||
      !absl::c_any_of(kMigrationPhases, [&pbs_value_column_migration_phase](
                                            absl::string_view valid_phase) {
        return pbs_value_column_migration_phase == valid_phase;
      })) {
    pbs_value_column_migration_phase = std::string(kMigrationPhase1);
    SCP_WARNING(kBinaryBudgetConsumer, kZeroUuid,
                absl::StrFormat("Invalid value for %s config key. Defaulting "
                                "to phase_1. Provided value: %s",
                                kValueProtoMigrationPhase,
                                pbs_value_column_migration_phase));
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
}

ExecutionResult BinaryBudgetConsumer::ParseTransactionRequest(
    const AuthContext& auth_context, const HttpHeaders& request_headers,
    const ConsumePrivacyBudgetRequest& request_proto) {
  if (auth_context.authorized_domain == nullptr) {
    SCP_INFO(kBinaryBudgetConsumer, kZeroUuid, "No auth context found");
    return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
  }
  absl::string_view authorized_domain = *auth_context.authorized_domain;

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  absl::flat_hash_set<std::string> visited;

  auto key_body_processor =
      [this, &visited](
          const ConsumePrivacyBudgetRequest::PrivacyBudgetKey& key_body,
          const size_t key_index,
          absl::string_view reporting_origin) -> ExecutionResult {
    const std::string& reporting_time = key_body.reporting_time();
    if (key_body.key().empty() || reporting_time.empty()) {
      SCP_INFO(kBinaryBudgetConsumer, kZeroUuid,
               "Either one of them is empty : \"key\" or \"reporting_time\"");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    const std::string budget_key =
        absl::StrCat(reporting_origin, "/", key_body.key());

    auto reporting_timestamp = ReportingTimeToTimeBucket(reporting_time);
    if (!reporting_timestamp.Successful()) {
      SCP_INFO(kBinaryBudgetConsumer, kZeroUuid, "Invalid reporting time");
      return reporting_timestamp.result();
    }

    TimeGroup time_group = Utils::GetTimeGroup(*reporting_timestamp);
    TimeBucket time_bucket = Utils::GetTimeBucket(*reporting_timestamp);

    std::string visited_key =
        absl::StrCat(budget_key, "_", time_group, "_", time_bucket);
    if (visited.find(visited_key) != visited.end()) {
      SCP_INFO(kBinaryBudgetConsumer, kZeroUuid,
               absl::StrFormat("Repeated key found : %s", visited_key))
      return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
    }
    visited.emplace(visited_key);

    // Binary budget consumption
    if (key_body.budget_type() != PrivacyBudgetKey::BUDGET_TYPE_UNSPECIFIED &&
        key_body.budget_type() != PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET) {
      SCP_INFO(kBinaryBudgetConsumer, kZeroUuid,
               absl::StrFormat(
                   "Expected binar or unspecified budget type, found %s",
                   PrivacyBudgetKey::BudgetType_Name(key_body.budget_type())));
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    int32_t token = key_body.token();
    if (token == 0 && key_body.tokens().empty()) {
      SCP_INFO(kBinaryBudgetConsumer, kZeroUuid,
               "Both \"token\" with non zero value and \"tokens\" are empty");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (token != 0 && !key_body.tokens().empty()) {
      SCP_INFO(kBinaryBudgetConsumer, kZeroUuid,
               "Both \"token\" with non zero value and \"tokens\" are present");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (token == 0 && key_body.tokens().size() != 1) {
      SCP_INFO(kBinaryBudgetConsumer, kZeroUuid, "\"tokens\" is not of size 1");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    token = token == 0 ? key_body.tokens(0).token_int32() : token;

    if (token != static_cast<int32_t>(kFullBudgetCount)) {
      SCP_INFO(kBinaryBudgetConsumer, kZeroUuid,
               absl::StrFormat("Expected token equals %d, found %d",
                               kFullBudgetCount, token));
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    PbsPrimaryKey pbs_primary_key(budget_key, std::to_string(time_group));
    ConsumptionState& consumption_state = metadata_[pbs_primary_key];
    consumption_state.hour_of_day_to_key_index_map[time_bucket] = key_index;
    ++key_count_;

    return SuccessExecutionResult();
  };

  return ParseCommonV2TransactionRequestProto(authorized_domain, request_proto,
                                              std::move(key_body_processor));
}

size_t BinaryBudgetConsumer::GetKeyCount() {
  return key_count_;
}

spanner::KeySet BinaryBudgetConsumer::GetSpannerKeySet() {
  spanner::KeySet spanner_key_set;
  for (const auto& [pbs_primary_key, consumption_state] : metadata_) {
    spanner_key_set.AddKey(pbs_primary_key.ToSpannerKey());
  }
  return spanner_key_set;
}

ExecutionResultOr<std::vector<std::string>>
BinaryBudgetConsumer::GetReadColumns() {
  std::vector<std::string> columns = {std::string(kBudgetTableBudgetKeyColumn),
                                      std::string(kBudgetTableTimeframeColumn)};
  if (enable_read_truth_from_value_column_) {
    columns.emplace_back(kBudgetTableValueSpannerColumn);
  } else {
    columns.emplace_back(kBudgetTableValueProtoColumn);
  }
  return columns;
}

template <typename TokenMetadataType>
  requires(std::is_same_v<TokenMetadataType, spanner::Json> ||
           std::is_same_v<TokenMetadataType, privacy_sandbox_pbs::BudgetValue>)
SpannerMutationsResult
BinaryBudgetConsumer::MutateConsumptionStateForKeysPresentInDatabase(
    spanner::RowStream& row_stream) {
  using TokenMetadataTypeBaseT = std::decay_t<TokenMetadataType>;
  using ValueColumnType = std::conditional_t<
      std::is_same_v<TokenMetadataTypeBaseT, spanner::Json>, spanner::Json,
      spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>>;
  using RowType = std::tuple<std::string, std::string, ValueColumnType>;

  SpannerMutationsResult spanner_mutations_result{
      .status = google::cloud::Status(),
      .execution_result = SuccessExecutionResult(),
      .budget_exhausted_indices = std::vector<size_t>(),
      .mutations = spanner::Mutations(),
  };

  for (const auto& row :
       google::cloud::spanner::StreamOf<RowType>(row_stream)) {
    if (!row) {
      spanner_mutations_result.status = google::cloud::Status(
          google::cloud::StatusCode::kInvalidArgument,
          absl::StrFormat("Error reading rows from the database. Reason: %s",
                          row.status().message()));
      spanner_mutations_result.execution_result =
          FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR);
      return spanner_mutations_result;
    }
    if (row.status().code() == google::cloud::StatusCode::kNotFound) {
      continue;
    }

    PbsPrimaryKey pbs_primary_key(std::get<0>(*row), std::get<1>(*row));
    auto metadata_it = metadata_.find(pbs_primary_key);
    if (metadata_it == metadata_.end()) {
      SCP_INFO(kBinaryBudgetConsumer, kZeroUuid,
               absl::StrFormat("Found key from database read call which was "
                               "not requested. Ignoring key : %s",
                               pbs_primary_key));
      continue;
    }
    ConsumptionState& consumption_state = metadata_it->second;
    consumption_state.is_key_already_present_in_database = true;

    if constexpr (std::is_same_v<TokenMetadataTypeBaseT, spanner::Json>) {
      std::tie(spanner_mutations_result.status,
               spanner_mutations_result.execution_result,
               consumption_state.budget_state) =
          ParseSpannerJson(std::get<2>(*row));

      if (!spanner_mutations_result.execution_result.Successful()) {
        spanner_mutations_result.status = google::cloud::Status(
            google::cloud::StatusCode::kInvalidArgument,
            absl::StrFormat(
                "Failed to parse spanner json string. Spanner JSON %s",
                std::string(std::get<2>(*row))));
        spanner_mutations_result.execution_result =
            FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR);
        return spanner_mutations_result;
      }
    } else {
      privacy_sandbox_pbs::BudgetValue budget_value(std::get<2>(*row));

      std::tie(spanner_mutations_result.status,
               spanner_mutations_result.execution_result) =
          VerifyLaplaceProto(budget_value);
      if (!spanner_mutations_result.execution_result.Successful()) {
        return spanner_mutations_result;
      }

      for (size_t i = 0; i < kDefaultTokenCountSize; ++i) {
        const int32_t budget = budget_value.laplace_dp_budgets().budgets(i);
        if (budget != kEmptyBudgetCount &&
            budget != kDefaultLaplaceDpBudgetCount) {
          spanner_mutations_result.status = google::cloud::Status(
              google::cloud::StatusCode::kInvalidArgument,
              absl::StrFormat("LaplaceDpBudgets value should be "
                              "either %d (full) or %d (empty), found %d",
                              kDefaultLaplaceDpBudgetCount, kEmptyBudgetCount,
                              budget));
          spanner_mutations_result.execution_result =
              FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR);
          return spanner_mutations_result;
        }
        consumption_state.budget_state[i] =
            budget == kDefaultLaplaceDpBudgetCount ? kFullBudgetCount
                                                   : kEmptyBudgetCount;
      }
    }

    for (const auto& [hour, key_index] :
         consumption_state.hour_of_day_to_key_index_map) {
      if (consumption_state.budget_state[hour] == kEmptyBudgetCount) {
        spanner_mutations_result.budget_exhausted_indices.push_back(key_index);
      }
      consumption_state.budget_state[hour] = kEmptyBudgetCount;
    }
  }

  if (!spanner_mutations_result.budget_exhausted_indices.empty()) {
    // To maintain backward compatibility
    std::sort(spanner_mutations_result.budget_exhausted_indices.begin(),
              spanner_mutations_result.budget_exhausted_indices.end());
    spanner_mutations_result.status = google::cloud::Status(
        google::cloud::StatusCode::kInvalidArgument, "Not enough budget.");
    spanner_mutations_result.execution_result =
        FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED);
    return spanner_mutations_result;
  }

  return spanner_mutations_result;
}

void BinaryBudgetConsumer::MutateConsumptionStateForKeysNotPresentInDatabase() {
  for (auto& [pbs_primary_key, consumption_state] : metadata_) {
    if (consumption_state.is_key_already_present_in_database) {
      continue;
    }

    consumption_state.budget_state.fill(kFullBudgetCount);
    for (const auto& [hour, _] :
         consumption_state.hour_of_day_to_key_index_map) {
      consumption_state.budget_state[hour] = kEmptyBudgetCount;
    }
  }
}

spanner::Mutations BinaryBudgetConsumer::GenerateSpannerMutations(
    absl::string_view table_name) {
  std::vector<std::string> columns = {
      std::string(kBudgetTableBudgetKeyColumn),
      std::string(kBudgetTableTimeframeColumn),
  };
  if (enable_write_to_value_column_) {
    columns.emplace_back(kBudgetTableValueSpannerColumn);
  }
  if (enable_write_to_value_proto_column_) {
    columns.emplace_back(kBudgetTableValueProtoColumn);
  }

  auto insert_or_update_builder =
      spanner::InsertOrUpdateMutationBuilder(std::string(table_name), columns);

  for (const auto& [pbs_primary_key, consumption_state] : metadata_) {
    std::vector<spanner::Value> values = {
        spanner::Value(pbs_primary_key.GetBudgetKey()),
        spanner::Value(pbs_primary_key.GetTimeframe())};

    if (enable_write_to_value_column_) {
      values.emplace_back(CreateSpannerJson(consumption_state.budget_state));
    }

    if (enable_write_to_value_proto_column_) {
      values.emplace_back(CreateLaplaceProto(consumption_state.budget_state));
    }

    insert_or_update_builder.AddRow(values);
  }

  return spanner::Mutations{insert_or_update_builder.Build()};
}

SpannerMutationsResult BinaryBudgetConsumer::ConsumeBudget(
    spanner::RowStream& row_stream, absl::string_view table_name) {
  SpannerMutationsResult spanner_mutations_result{
      .status = google::cloud::Status(),
      .execution_result = SuccessExecutionResult(),
      .budget_exhausted_indices = std::vector<size_t>(),
      .mutations = spanner::Mutations(),
  };

  if (enable_read_truth_from_value_column_) {
    spanner_mutations_result =
        MutateConsumptionStateForKeysPresentInDatabase<spanner::Json>(
            row_stream);
  } else {
    spanner_mutations_result = MutateConsumptionStateForKeysPresentInDatabase<
        privacy_sandbox_pbs::BudgetValue>(row_stream);
  }

  if (!spanner_mutations_result.execution_result.Successful()) {
    SCP_INFO(kBinaryBudgetConsumer, kZeroUuid,
             absl::StrFormat("Failed to multate consumption state. Reason: %s",
                             spanner_mutations_result.status.message()));
    return spanner_mutations_result;
  }

  MutateConsumptionStateForKeysNotPresentInDatabase();
  spanner_mutations_result.mutations = GenerateSpannerMutations(table_name);

  return spanner_mutations_result;
}

std::vector<std::string> BinaryBudgetConsumer::DebugKeyList() {
  std::vector<std::string> result(key_count_);
  size_t index = 0;
  for (const auto& [pbs_primary_key, consumption_state] : metadata_) {
    for (const auto& [hour, _] :
         consumption_state.hour_of_day_to_key_index_map) {
      std::string key_string = absl::StrFormat(
          "Budget Key: %s Day %s Hour %llu", pbs_primary_key.GetBudgetKey(),
          pbs_primary_key.GetTimeframe(), hour);
      result[index] = key_string;
      ++index;
    }
  }

  return result;
}

}  // namespace privacy_sandbox::pbs
