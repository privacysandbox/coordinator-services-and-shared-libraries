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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/strings/str_join.h"
#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/core/interface/http_types.h"
#include "cc/pbs/consume_budget/src/budget_consumer.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/mocks/row.h"
#include "google/protobuf/text_format.h"
#include "proto/pbs/api/v1/api.pb.h"

namespace privacy_sandbox::pbs {
namespace {

using ::google::protobuf::TextFormat;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest;
using ::privacy_sandbox::pbs_common::AuthContext;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::MockConfigProvider;
using ::privacy_sandbox::pbs_common::ResultIs;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
namespace spanner_mocks = ::google::cloud::spanner_mocks;
namespace spanner = ::google::cloud::spanner;

constexpr absl::string_view kMigrationPhase1 = "phase_1";
constexpr absl::string_view kMigrationPhase2 = "phase_2";
constexpr absl::string_view kMigrationPhase3 = "phase_3";
constexpr absl::string_view kMigrationPhase4 = "phase_4";

constexpr absl::string_view kBudgetKeySpannerColumnName = "Budget_Key";
constexpr absl::string_view kTimeframeSpannerColumnName = "Timeframe";
constexpr absl::string_view kValueSpannerColumnName = "Value";
constexpr absl::string_view kValueProtoSpannerColumnName = "ValueProto";
constexpr int32_t kDefaultLaplaceDpBudgetCount = 6400;
constexpr int8_t kFullBudgetCount = 1;
constexpr int8_t kEmptyBudgetCount = 0;
constexpr size_t kDefaultTokenCountSize = 24;
constexpr size_t k20191211DaysFromEpoch = 18241;

constexpr absl::string_view kAuthorizedDomain = "https://fake.com";
constexpr absl::string_view kTableName = "fake-table-name";

class BinaryBudgetConsumerTest
    : public testing::Test,
      public testing::WithParamInterface<absl::string_view> {
 protected:
  void SetUp() override {
    auto mock_config_provider = std::make_unique<MockConfigProvider>();
    mock_config_provider->Set(kValueProtoMigrationPhase, GetMigrationPhase());

    binary_budget_consumer_ =
        std::make_unique<BinaryBudgetConsumer>(mock_config_provider.get());
    mock_source_ = std::make_unique<spanner_mocks::MockResultSetSource>();
  }

  std::string GetMigrationPhase() { return std::string(GetParam()); }

  bool WriteValueColumn() {
    return (GetMigrationPhase() == kMigrationPhase1 ||
            GetMigrationPhase() == kMigrationPhase2 ||
            GetMigrationPhase() == kMigrationPhase3);
  }

  bool WriteValueProtoColumn() {
    return (GetMigrationPhase() == kMigrationPhase2 ||
            GetMigrationPhase() == kMigrationPhase3 ||
            GetMigrationPhase() == kMigrationPhase4);
  }

  bool ReadFromValueColumn() {
    return (GetMigrationPhase() == kMigrationPhase1 ||
            GetMigrationPhase() == kMigrationPhase2);
  }

  std::vector<std::string> GetTableColumns() {
    std::vector<std::string> columns = {
        std::string(kBudgetKeySpannerColumnName),
        std::string(kTimeframeSpannerColumnName),
    };

    if (WriteValueColumn()) {
      columns.emplace_back(kValueSpannerColumnName);
    }
    if (WriteValueProtoColumn()) {
      columns.emplace_back(kValueProtoSpannerColumnName);
    }

    return columns;
  }

  spanner::Json GetSpannerJson(const absl::Span<const int8_t> token_count) {
    std::vector<std::string> token_count_strings;
    std::transform(token_count.begin(), token_count.end(),
                   std::back_inserter(token_count_strings),
                   [](int count) { return std::to_string(count); });
    std::string serialized_token_count =
        absl::StrJoin(token_count_strings, " ");
    std::string json =
        absl::StrFormat(R"({"TokenCount":"%s"})", serialized_token_count);
    return spanner::Json(json);
  }

  spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue> GetProtoMessage(
      const absl::Span<const int8_t> token_count) {
    privacy_sandbox_pbs::BudgetValue budget_value;
    budget_value.mutable_laplace_dp_budgets()->mutable_budgets()->Reserve(
        token_count.size());

    privacy_sandbox_pbs::BudgetValue::LaplaceDpBudgets* dp_budgets =
        budget_value.mutable_laplace_dp_budgets();
    for (const auto& token : token_count) {
      dp_budgets->add_budgets(token == kFullBudgetCount
                                  ? kDefaultLaplaceDpBudgetCount
                                  : kEmptyBudgetCount);
    }
    return spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>(
        budget_value);
  }

  std::vector<spanner::Value> GetTableValues(
      absl::string_view budget_key, absl::string_view timeframe,
      const absl::Span<const int8_t> token_count) {
    std::vector<spanner::Value> values = {
        spanner::Value(std::string(budget_key)),
        spanner::Value(std::string(timeframe)),
    };

    if (WriteValueColumn()) {
      values.emplace_back(GetSpannerJson(token_count));
    }

    if (WriteValueProtoColumn()) {
      values.emplace_back(GetProtoMessage(token_count));
    }

    return values;
  }

  std::vector<std::pair<std::string, spanner::Value>> GetRowPairsForNextRow(
      absl::string_view budget_key, absl::string_view timeframe,
      const absl::Span<const int8_t> token_count) {
    std::vector<std::pair<std::string, spanner::Value>> rows;
    EXPECT_EQ(token_count.size(), kDefaultTokenCountSize);

    rows.emplace_back(kBudgetKeySpannerColumnName, std::string(budget_key));
    rows.emplace_back(kTimeframeSpannerColumnName, std::string(timeframe));

    if (ReadFromValueColumn()) {
      rows.emplace_back(kValueSpannerColumnName, GetSpannerJson(token_count));
    } else {
      rows.emplace_back(kValueProtoSpannerColumnName,
                        GetProtoMessage(token_count));
    }

    return rows;
  }

  HttpHeaders GetHeaders(const std::string& transaction_origin) {
    return {{kTransactionOriginHeader, transaction_origin}};
  }

  spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>
  GetProtoValueWithInvalidTokens(absl::Span<const int32_t> token_count) {
    privacy_sandbox_pbs::BudgetValue budget_value;
    budget_value.mutable_laplace_dp_budgets()->mutable_budgets()->Reserve(
        token_count.size());
    privacy_sandbox_pbs::BudgetValue::LaplaceDpBudgets* dp_budgets =
        budget_value.mutable_laplace_dp_budgets();
    for (const auto& token : token_count) {
      dp_budgets->add_budgets(token);
    }
    return budget_value;
  }

  AuthContext GetAuthContext() {
    return {
        .authorized_domain = std::make_shared<std::string>(kAuthorizedDomain),
    };
  }

  std::unique_ptr<BinaryBudgetConsumer> binary_budget_consumer_;
  std::unique_ptr<spanner_mocks::MockResultSetSource> mock_source_;
};

INSTANTIATE_TEST_SUITE_P(BinaryBudgetConsumerTest, BinaryBudgetConsumerTest,
                         Values(kMigrationPhase1, kMigrationPhase2,
                                kMigrationPhase3, kMigrationPhase4));

TEST_P(BinaryBudgetConsumerTest, ValidRequestBodyV2Success) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys {
        key: "234"
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T07:20:50.52Z"
      }
    }
    data {
      reporting_origin: "http://b.fake.com"
      keys { key: "234" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  // Unfortunately we cannot compare spanner key sets (seems they are ordered
  // sets and metadata is a hash set). So, this is an hack around
  std::vector<std::string> expected_keys_list{
      absl::StrCat("Budget Key: http://a.fake.com/123", " Day ",
                   std::to_string(k20191211DaysFromEpoch), " Hour 7"),
      absl::StrCat("Budget Key: http://a.fake.com/234", " Day ",
                   std::to_string(k20191211DaysFromEpoch), " Hour 7"),
      absl::StrCat("Budget Key: http://b.fake.com/234", " Day ",
                   std::to_string(k20191211DaysFromEpoch + 1), " Hour 7")};
  EXPECT_THAT(binary_budget_consumer_->DebugKeyList(),
              UnorderedElementsAreArray(expected_keys_list));
  EXPECT_EQ(binary_budget_consumer_->GetKeyCount(), expected_keys_list.size());
}

TEST_P(BinaryBudgetConsumerTest,
       RepeatedKeyButDifferentReportingTimeDateValidRequestBodyV2Success) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys {
        key: "123"
        tokens { token_int32: 1 }
        reporting_time: "2019-12-12T07:20:50.52Z"
      }
    }
    data {
      reporting_origin: "http://b.fake.com"
      keys { key: "234" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  // Unfortunately we cannot compare spanner key sets (seems they are ordered
  // sets and metadata is a hash set). So, this is an hack around
  std::vector<std::string> expected_keys_list{
      absl::StrCat("Budget Key: http://a.fake.com/123", " Day ",
                   std::to_string(k20191211DaysFromEpoch), " Hour 7"),
      absl::StrCat("Budget Key: http://a.fake.com/123", " Day ",
                   std::to_string(k20191211DaysFromEpoch + 1), " Hour 7"),
      absl::StrCat("Budget Key: http://b.fake.com/234", " Day ",
                   std::to_string(k20191211DaysFromEpoch + 1), " Hour 7")};
  EXPECT_THAT(binary_budget_consumer_->DebugKeyList(),
              UnorderedElementsAreArray(expected_keys_list));
  EXPECT_EQ(binary_budget_consumer_->GetKeyCount(), expected_keys_list.size());
}

TEST_P(BinaryBudgetConsumerTest,
       RepeatedKeyButDifferentReportingTimeHourValidRequestBodyV2Success) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys {
        key: "123"
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T08:20:50.52Z"
      }
    }
    data {
      reporting_origin: "http://b.fake.com"
      keys { key: "234" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  // Unfortunately we cannot compare spanner key sets (seems they are ordered
  // sets and metadata is a hash set). So, this is an hack around
  std::vector<std::string> expected_keys_list{
      absl::StrCat("Budget Key: http://a.fake.com/123", " Day ",
                   std::to_string(k20191211DaysFromEpoch), " Hour 7"),
      absl::StrCat("Budget Key: http://a.fake.com/123", " Day ",
                   std::to_string(k20191211DaysFromEpoch), " Hour 8"),
      absl::StrCat("Budget Key: http://b.fake.com/234", " Day ",
                   std::to_string(k20191211DaysFromEpoch + 1), " Hour 7")};
  EXPECT_THAT(binary_budget_consumer_->DebugKeyList(),
              UnorderedElementsAreArray(expected_keys_list));
  EXPECT_EQ(binary_budget_consumer_->GetKeyCount(), expected_keys_list.size());
}

TEST_P(BinaryBudgetConsumerTest,
       RepeatedKeyButDifferentReportingTimeMinutesValidRequestBodyV2Failure) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys {
        key: "123"
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T07:21:50.52Z"
      }
    }
    data {
      reporting_origin: "http://b.fake.com"
      keys { key: "234" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result,
              FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST));
}

TEST_P(BinaryBudgetConsumerTest, MissingKeyInRequestBody) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys { token: 1, reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(BinaryBudgetConsumerTest, RepeatedKeyInRequestBody) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(FailureExecutionResult(
                                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

TEST_P(BinaryBudgetConsumerTest, MissingReportingTimeInRequestBody) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys { key: "124", token: 1 }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(BinaryBudgetConsumerTest, InvalidReportingTimeInRequestBody) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://b.fake.com"
      keys { key: "234", token: 1, reporting_time: "invalid" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(FailureExecutionResult(
                                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

TEST_P(BinaryBudgetConsumerTest, MissingTokenInRequestBody) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys { key: "234", reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(BinaryBudgetConsumerTest, TokenAndTokensInRequestBody) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys {
        key: "234"
        token: 1
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T07:20:50.52Z"
      }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(BinaryBudgetConsumerTest, EmptyTokensInRequestBody) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys {
        key: "234"
        tokens {}
        reporting_time: "2019-12-11T07:20:50.52Z"
      }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(BinaryBudgetConsumerTest, MultipleTokensInRequestBody) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys {
        key: "234"
        tokens { token_int32: 1 }
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T07:20:50.52Z"
      }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(BinaryBudgetConsumerTest, InvalidTokensInRequestBody) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys {
        key: "234"
        tokens { token_int32: 2 }
        reporting_time: "2019-12-11T07:20:50.52Z"
      }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

// Spanner mutations are ordered. Since we use absl::flat_hash_map, the order
// of the keys (and thus the order of the mutations) are not same as the
// request. Thus we test only one key when comparing expected mutations and
// actual mutations.

TEST_P(BinaryBudgetConsumerTest, ValidRequestBodyV2SuccessfulMutations) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys {
        key: "234"
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T07:20:50.52Z"
      }
    }
    data {
      reporting_origin: "http://b.fake.com"
      keys { key: "234" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  // Entries present with budget consumed at the first hour
  std::array<int8_t, kDefaultTokenCountSize> token_count;
  token_count.fill(kFullBudgetCount);
  token_count[0] = kEmptyBudgetCount;

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(
          "http://a.fake.com/123", std::to_string(k20191211DaysFromEpoch),
          token_count))))
      .WillOnce(Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(
          "http://a.fake.com/234", std::to_string(k20191211DaysFromEpoch),
          token_count))))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_SUCCESS(spanner_mutations_result.execution_result);
  EXPECT_TRUE(spanner_mutations_result.status.ok());
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest, BudgetConsumptionOnExistingRowShouldSuccess) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  // Entries present with budget consumed at the first hour
  std::array<int8_t, kDefaultTokenCountSize> token_count;
  token_count.fill(kFullBudgetCount);
  token_count[0] = kEmptyBudgetCount;

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(
          "http://a.fake.com/123", std::to_string(k20191211DaysFromEpoch),
          token_count))))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  // consume budget for 7th hour
  token_count[7] = kEmptyBudgetCount;

  auto insert_or_update_builder =
      spanner::InsertOrUpdateMutationBuilder(std::string(kTableName),
                                             GetTableColumns())
          .AddRow(GetTableValues("http://a.fake.com/123",
                                 std::to_string(k20191211DaysFromEpoch),
                                 token_count));

  spanner::Mutations expected_mutations{insert_or_update_builder.Build()};
  EXPECT_THAT(spanner_mutations_result.mutations,
              UnorderedElementsAreArray(expected_mutations));

  EXPECT_SUCCESS(spanner_mutations_result.execution_result);
  EXPECT_TRUE(spanner_mutations_result.status.ok());
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest,
       BudgetConsumptionOnNonExistingRowShouldSuccess) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  EXPECT_CALL(*mock_source_, NextRow()).WillRepeatedly(Return(spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  // consume budget for 7th hour
  std::array<int8_t, kDefaultTokenCountSize> token_count;
  token_count.fill(kFullBudgetCount);
  token_count[7] = kEmptyBudgetCount;

  auto insert_or_update_builder =
      spanner::InsertOrUpdateMutationBuilder(std::string(kTableName),
                                             GetTableColumns())
          .AddRow(GetTableValues("http://a.fake.com/123",
                                 std::to_string(k20191211DaysFromEpoch),
                                 token_count));

  spanner::Mutations expected_mutations{insert_or_update_builder.Build()};
  EXPECT_THAT(spanner_mutations_result.mutations,
              UnorderedElementsAreArray(expected_mutations));

  EXPECT_SUCCESS(spanner_mutations_result.execution_result);
  EXPECT_TRUE(spanner_mutations_result.status.ok());
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest, BudgetConsumptionWithoutBudget) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  // consume budget for 7th hour
  std::array<int8_t, kDefaultTokenCountSize> token_count;
  token_count.fill(kFullBudgetCount);
  token_count[7] = kEmptyBudgetCount;

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(
          "http://a.fake.com/123", std::to_string(k20191211DaysFromEpoch),
          token_count))))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(spanner_mutations_result.execution_result,
              ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_THAT(spanner_mutations_result.budget_exhausted_indices,
              ElementsAre(0));
}

TEST_P(BinaryBudgetConsumerTest,
       BudgetConsumptionWithoutBudgetForMultipleKeys) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys {
        key: "234"
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T07:20:50.52Z"
      }
    }
    data {
      reporting_origin: "http://b.fake.com"
      keys { key: "234" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  // consume budget for 7th hour
  std::array<int8_t, kDefaultTokenCountSize> token_count;
  token_count.fill(kFullBudgetCount);
  token_count[7] = kEmptyBudgetCount;

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(
          "http://a.fake.com/123", std::to_string(k20191211DaysFromEpoch),
          token_count))))
      .WillOnce(Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(
          "http://a.fake.com/234", std::to_string(k20191211DaysFromEpoch),
          token_count))))
      .WillOnce(Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(
          "http://b.fake.com/234", std::to_string(k20191211DaysFromEpoch + 1),
          token_count))))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(spanner_mutations_result.execution_result,
              ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_THAT(spanner_mutations_result.budget_exhausted_indices,
              ElementsAre(0, 1, 2));
}

TEST_P(BinaryBudgetConsumerTest,
       BudgetConsumptionWithoutBudgetForSameKeyDifferentHours) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys {
        key: "123"
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T08:20:50.52Z"
      }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  // consume budget for 7th hour
  std::array<int8_t, kDefaultTokenCountSize> token_count;
  token_count.fill(kFullBudgetCount);
  token_count[7] = kEmptyBudgetCount;
  token_count[8] = kEmptyBudgetCount;

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(
          "http://a.fake.com/123", std::to_string(k20191211DaysFromEpoch),
          token_count))))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(spanner_mutations_result.execution_result,
              ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_THAT(spanner_mutations_result.budget_exhausted_indices,
              ElementsAre(0, 1));
}

TEST_P(BinaryBudgetConsumerTest, BudgetConsumptionWithInvalidRow) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(
          Return(spanner_mocks::MakeRow({{"abc", spanner::Value("def")}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(
      spanner_mutations_result.execution_result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest, BudgetConsumptionWithInvalidJsonValueColumn) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("http://a.fake.com/123")},
           {std::string(kTimeframeSpannerColumnName),
            spanner::Value(std::to_string(k20191211DaysFromEpoch))},
           {std::string(kValueSpannerColumnName),
            spanner::Value(
                spanner::Json(R"({"TokenCount": Invalid JSON format")"))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(
      spanner_mutations_result.execution_result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest,
       BudgetConsumptionWithoutTokenCountFieldInJson) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("http://a.fake.com/123")},
           {std::string(kTimeframeSpannerColumnName),
            spanner::Value(std::to_string(k20191211DaysFromEpoch))},
           {std::string(kValueSpannerColumnName),
            spanner::Value(spanner::Json(
                R"({"TokenCountFake": "No TokenCount field"})"))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(
      spanner_mutations_result.execution_result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest,
       BudgetConsumptionWithInvalidTokenCountFieldInJson) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("http://a.fake.com/123")},
           {std::string(kTimeframeSpannerColumnName),
            spanner::Value(std::to_string(k20191211DaysFromEpoch))},
           {std::string(kValueSpannerColumnName),
            spanner::Value(
                spanner::Json(R"({"TokenCount": "No TokenCount field"})"))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(
      spanner_mutations_result.execution_result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest, BudgetConsumptionWithNoLaplaceDp) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("http://a.fake.com/123")},
           {std::string(kTimeframeSpannerColumnName),
            spanner::Value(std::to_string(k20191211DaysFromEpoch))},
           {std::string(kValueSpannerColumnName),
            spanner::Value(
                spanner::ProtoMessage<privacy_sandbox_pbs::BudgetValue>(
                    privacy_sandbox_pbs::BudgetValue()))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(
      spanner_mutations_result.execution_result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest, BudgetConsumptionWithInvalidLaplaceDpSize) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("http://a.fake.com/123")},
           {std::string(kTimeframeSpannerColumnName),
            spanner::Value(std::to_string(k20191211DaysFromEpoch))},
           {std::string(kValueSpannerColumnName),
            spanner::Value(GetProtoValueWithInvalidTokens({1, 1, 1}))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(
      spanner_mutations_result.execution_result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest, BudgetConsumptionWithInvalidLaplaceDpTokens) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  std::vector<int32_t> token_count(kDefaultTokenCountSize,
                                   kDefaultLaplaceDpBudgetCount);
  token_count[0] -= 1;  // Making an invalid entry

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(
          {{std::string(kBudgetKeySpannerColumnName),
            spanner::Value("http://a.fake.com/123")},
           {std::string(kTimeframeSpannerColumnName),
            spanner::Value(std::to_string(k20191211DaysFromEpoch))},
           {std::string(kValueSpannerColumnName),
            spanner::Value(GetProtoValueWithInvalidTokens(token_count))}})))
      .WillRepeatedly(Return(google::cloud::spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  EXPECT_TRUE(spanner_mutations_result.mutations.empty());
  EXPECT_THAT(
      spanner_mutations_result.execution_result,
      ResultIs(FailureExecutionResult(SC_CONSUME_BUDGET_PARSING_ERROR)));
  EXPECT_EQ(spanner_mutations_result.status.code(),
            google::cloud::StatusCode::kInvalidArgument);
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(
    BinaryBudgetConsumerTest,
    BudgetConsumptionWithSameKeyButDifferentHoursOnExistingRowShouldSucceed) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys {
        key: "123"
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T08:20:50.52Z"
      }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  // Entries present with budget consumed at the first hour
  std::array<int8_t, kDefaultTokenCountSize> token_count;
  token_count.fill(kFullBudgetCount);
  token_count[0] = kEmptyBudgetCount;

  EXPECT_CALL(*mock_source_, NextRow())
      .WillOnce(Return(spanner_mocks::MakeRow(GetRowPairsForNextRow(
          "http://a.fake.com/123", std::to_string(k20191211DaysFromEpoch),
          token_count))))
      .WillRepeatedly(Return(spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  // consume budget for 7th and 8th hour
  token_count[7] = kEmptyBudgetCount;
  token_count[8] = kEmptyBudgetCount;

  auto insert_or_update_builder =
      spanner::InsertOrUpdateMutationBuilder(std::string(kTableName),
                                             GetTableColumns())
          .AddRow(GetTableValues("http://a.fake.com/123",
                                 std::to_string(k20191211DaysFromEpoch),
                                 token_count));

  spanner::Mutations expected_mutations{insert_or_update_builder.Build()};
  EXPECT_THAT(spanner_mutations_result.mutations,
              UnorderedElementsAreArray(expected_mutations));

  EXPECT_SUCCESS(spanner_mutations_result.execution_result);
  EXPECT_TRUE(spanner_mutations_result.status.ok());
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

TEST_P(BinaryBudgetConsumerTest,
       BudgetConsumptionWithSameKeyButDiffHoursOnNonExistingRowSucceed) {
  absl::string_view request_body_proto = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
      keys {
        key: "123"
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T08:20:50.52Z"
      }
    }
  )pb";
  ConsumePrivacyBudgetRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_body_proto, &request));

  ExecutionResult execution_result =
      binary_budget_consumer_->ParseTransactionRequest(GetAuthContext(),
                                                       HttpHeaders{}, request);
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));

  EXPECT_CALL(*mock_source_, NextRow()).WillRepeatedly(Return(spanner::Row()));

  spanner::RowStream row_stream(std::move(mock_source_));
  SpannerMutationsResult spanner_mutations_result =
      binary_budget_consumer_->ConsumeBudget(row_stream, kTableName);

  // consume budget for 7th and 8th hour
  std::array<int8_t, kDefaultTokenCountSize> token_count;
  token_count.fill(kFullBudgetCount);
  token_count[7] = kEmptyBudgetCount;
  token_count[8] = kEmptyBudgetCount;

  auto insert_or_update_builder =
      spanner::InsertOrUpdateMutationBuilder(std::string(kTableName),
                                             GetTableColumns())
          .AddRow(GetTableValues("http://a.fake.com/123",
                                 std::to_string(k20191211DaysFromEpoch),
                                 token_count));

  spanner::Mutations expected_mutations{insert_or_update_builder.Build()};
  EXPECT_THAT(spanner_mutations_result.mutations,
              UnorderedElementsAreArray(expected_mutations));

  EXPECT_SUCCESS(spanner_mutations_result.execution_result);
  EXPECT_TRUE(spanner_mutations_result.status.ok());
  EXPECT_TRUE(spanner_mutations_result.budget_exhausted_indices.empty());
}

}  // namespace
}  // namespace privacy_sandbox::pbs
