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

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

#include "absl/log/check.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/config_provider/src/env_config_provider.h"
#include "cc/core/http2_client/src/synchronous_http2_client.h"
#include "cc/core/interface/errors.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/core/test/utils/proto_test_utils.h"
#include "cc/pbs/interface/type_def.h"
#include "gmock/gmock.h"
#include "proto/pbs/api/v1/api.pb.h"

namespace privacy_sandbox::pbs_common {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::google::protobuf::RepeatedField;
using ::google::protobuf::RepeatedPtrField;
using ::google::protobuf::TextFormat;
using ::google::protobuf::json::JsonStringToMessage;
using ::google::protobuf::util::MessageToJsonString;
using ::privacy_sandbox::pbs::kTransactionIdHeader;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetResponse;
using ::testing::ExplainMatchResult;
using ::testing::Pointee;
using ::testing::PrintToString;
using ::testing::SizeIs;
using ::testing::Test;

constexpr absl::string_view kCoordinatorUrl = "COORDINATOR_URL";
constexpr absl::string_view kKokoroBuildId = "KOKORO_BUILD_ID";
constexpr absl::string_view kKokoroBuildNum = "KOKORO_BUILD_NUM";
constexpr absl::string_view kClaimIdentity = "CLAIMED_IDENTITY";
constexpr absl::string_view kUserAgent = "USER_AGENT";
constexpr absl::string_view kGcpToken = "GCP_IDENTITY_TOKEN";
constexpr absl::string_view kHttpTag = "http://";
constexpr absl::string_view kHttpsTag = "https://";
constexpr TimeDuration kHttpClientBackoffDurationInMs = 10;
constexpr int kHttpClientMaxRetries = 6;
constexpr TimeDuration kHttp2ReadTimeoutInSeconds = 5;

struct BudgetConsumptionResponse {
  HttpStatusCode status_code;
  std::optional<ConsumePrivacyBudgetResponse> response_proto;

  // Useful for printing valuable information in Gtest about this struct
  friend std::ostream& operator<<(std::ostream& os,
                                  const BudgetConsumptionResponse& obj) {
    os << "HTTP Status code: " << HttpStatusCodeToString(obj.status_code)
       << " Proto: ";
    if (obj.response_proto.has_value()) {
      os << obj.response_proto.value().DebugString();
    } else {
      os << "No Proto";
    }
    return os;
  }
};

MATCHER_P(EqualsBudgetConsumptionResponse, expected,
          std::string(negation ? "is not" : "is") + " equal to " +
              PrintToString(expected)) {
  if (expected.status_code != arg.status_code) {
    *result_listener << "Expected HTTP status code: "
                     << HttpStatusCodeToString(expected.status_code)
                     << "\n Actual HTTP status code: "
                     << HttpStatusCodeToString(arg.status_code);
    return false;
  }

  bool do_expect_response_proto = expected.response_proto.has_value();
  bool do_actual_response_proto = arg.response_proto.has_value();

  if (do_expect_response_proto != do_actual_response_proto) {
    *result_listener << "Expeted proto presence: " << do_expect_response_proto
                     << "\n Actual proto presence: "
                     << do_actual_response_proto;
    return false;
  }

  // Compare proto if present or simply return true
  return do_expect_response_proto
             ? ExplainMatchResult(EqualsProto(expected.response_proto.value()),
                                  arg.response_proto.value(), result_listener)
             : true;
}

class PbsPostSubmitTest : public Test {
 protected:
  static void SetUpTestSuite() {
    EnvConfigProvider config_provider;
    CHECK(config_provider.Init());

    std::string coordinator_url;
    CHECK(config_provider.Get(std::string(kCoordinatorUrl), coordinator_url));
    pbs_consume_budget_url_ =
        absl::StrCat(coordinator_url, "/v1/transactions:consume-budget");

    HttpClientOptions http_client_options(
        RetryStrategyOptions(RetryStrategyType::Linear,
                             kHttpClientBackoffDurationInMs,
                             kHttpClientMaxRetries),
        kDefaultMaxConnectionsPerHost, kHttp2ReadTimeoutInSeconds);
    sync_http_client_ = std::make_unique<SyncHttpClient>(http_client_options);

    CHECK(config_provider.Get(std::string(kKokoroBuildId), kokoro_build_id_));
    CHECK(config_provider.Get(std::string(kKokoroBuildNum), kokoro_build_num_));
    CHECK(config_provider.Get(std::string(kClaimIdentity), claimed_identity_));
    CHECK(config_provider.Get(std::string(kUserAgent), user_agent_));
    CHECK(config_provider.Get(std::string(kGcpToken), token_));
    CHECK(config_provider.Stop());
  }

  absl::StatusOr<BudgetConsumptionResponse> PerformRequest(
      const ConsumePrivacyBudgetRequest& req) {
    absl::StatusOr<HttpRequest> http_request = MakeHttpRequest(req);
    if (!http_request.ok()) {
      return http_request.status();
    }

    SyncHttpClientResponse response =
        sync_http_client_->PerformRequest(*http_request);
    if (response.http_response == nullptr) {
      return absl::FailedPreconditionError("Failed to send request");
    }

    BudgetConsumptionResponse budget_consumption_response;
    budget_consumption_response.status_code = response.http_response->code;
    budget_consumption_response.response_proto = std::nullopt;

    if (response.http_response->body.bytes != nullptr &&
        response.http_response->body.bytes->size() > 0) {
      absl::StatusOr<ConsumePrivacyBudgetResponse> response_proto =
          ExtractProtoFromHttpResponse(response.http_response->body);
      if (!response_proto.ok()) {
        return response_proto.status();
      }
      budget_consumption_response.response_proto = std::move(*response_proto);
    }

    return budget_consumption_response;
  }

 private:
  absl::StatusOr<HttpRequest> MakeHttpRequest(
      const ConsumePrivacyBudgetRequest& request) {
    std::string request_body;
    if (absl::Status status = MessageToJsonString(request, &request_body);
        !status.ok()) {
      return status;
    }

    HttpRequest http_request;
    http_request.method = HttpMethod::POST;
    http_request.path = std::make_shared<Uri>(pbs_consume_budget_url_);
    http_request.headers = std::make_shared<HttpHeaders>();

    http_request.headers->insert({
        {kAuthHeader, token_},
        {kClaimedIdentityHeader, claimed_identity_},
        {kTransactionIdHeader, ToString(Uuid::GenerateUuid())},
        {kUserAgentHeader, user_agent_},
    });

    http_request.body.length = request_body.size();
    http_request.body.bytes = std::make_shared<std::vector<Byte>>(
        request_body.begin(), request_body.end());
    return http_request;
  }

  absl::StatusOr<ConsumePrivacyBudgetResponse> ExtractProtoFromHttpResponse(
      const BytesBuffer& body) {
    ConsumePrivacyBudgetResponse received_response_proto;
    absl::string_view request_body(body.bytes->begin(), body.bytes->end());
    if (absl::Status status =
            JsonStringToMessage(request_body, &received_response_proto);
        !status.ok()) {
      return status;
    }
    return received_response_proto;
  }

 protected:
  inline static std::string kokoro_build_id_;
  inline static std::string kokoro_build_num_;
  inline static std::string claimed_identity_;

 private:
  inline static std::unique_ptr<SyncHttpClient> sync_http_client_;
  inline static std::string pbs_consume_budget_url_;
  inline static std::string user_agent_;
  inline static std::string token_;
};

absl::StatusOr<std::string> GetReportingOrigin(
    absl::string_view origin, absl::string_view claimed_identity) {
  if (claimed_identity.starts_with(kHttpTag)) {
    return absl::StrCat(kHttpTag, origin, ".",
                        claimed_identity.substr(kHttpTag.size()));
  }
  if (claimed_identity.starts_with(kHttpsTag)) {
    return absl::StrCat(kHttpsTag, origin, ".",
                        claimed_identity.substr(kHttpsTag.size()));
  }
  return absl::InvalidArgumentError("Invalid claimed identity " +
                                    std::string(claimed_identity));
}

TEST_F(PbsPostSubmitTest, BinaryBudgetConsumption) {
  constexpr int kKeyCount = 50;
  constexpr int kDataCount = 3;

  ConsumePrivacyBudgetRequest consume_budget_request;
  consume_budget_request.set_version("2.0");

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  using BudgetRequestData = ConsumePrivacyBudgetRequest::BudgetRequestData;

  RepeatedPtrField<BudgetRequestData>* data_sections =
      consume_budget_request.mutable_data();
  data_sections->Reserve(kDataCount);

  for (int data_index = 0; data_index < kDataCount; ++data_index) {
    // Reporting origin if of the form
    // http<s>://pbsoriginbin<data-index>-<build-num>.<claimed-identity>
    absl::StatusOr<std::string> reporting_origin = GetReportingOrigin(
        absl::StrCat("pbsoriginbin", data_index, "-", kokoro_build_num_),
        claimed_identity_);
    ASSERT_THAT(reporting_origin, IsOk());

    BudgetRequestData* data_section = data_sections->Add();
    data_section->set_reporting_origin(*reporting_origin);

    RepeatedPtrField<PrivacyBudgetKey>* key_sections =
        data_section->mutable_keys();
    key_sections->Reserve(kKeyCount);

    for (int key_index = 0; key_index < kKeyCount; ++key_index) {
      PrivacyBudgetKey* key = key_sections->Add();
      key->set_key(absl::StrCat(kokoro_build_id_, "-", key_index));
      key->set_reporting_time(absl::FormatTime(
          "%Y-%m-%dT%H:%M:%E3SZ", absl::Now(), absl::UTCTimeZone()));
      key->set_budget_type(PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET);

      key->set_token(1);
    }
    ASSERT_THAT(key_sections, Pointee(SizeIs(kKeyCount)));
  }
  ASSERT_THAT(data_sections, Pointee(SizeIs(kDataCount)));

  const BudgetConsumptionResponse expected_response_success{
      .status_code = HttpStatusCode::OK,
      .response_proto = std::nullopt,
  };

  EXPECT_THAT(
      PerformRequest(consume_budget_request),
      IsOkAndHolds(EqualsBudgetConsumptionResponse(expected_response_success)));

  ConsumePrivacyBudgetResponse expected_response_proto;
  expected_response_proto.set_version("1.0");

  RepeatedField<::uint32_t>* budget_exhausted_indices =
      expected_response_proto.mutable_exhausted_budget_indices();
  budget_exhausted_indices->Reserve(kKeyCount * kDataCount);
  for (int index = 0; index < kKeyCount * kDataCount; ++index) {
    budget_exhausted_indices->Add(index);
  }

  const BudgetConsumptionResponse expected_response_failure{
      .status_code = HttpStatusCode::CONFLICT,
      .response_proto = std::move(expected_response_proto),
  };

  EXPECT_THAT(
      PerformRequest(consume_budget_request),
      IsOkAndHolds(EqualsBudgetConsumptionResponse(expected_response_failure)));
}

}  // namespace
}  // namespace privacy_sandbox::pbs_common
