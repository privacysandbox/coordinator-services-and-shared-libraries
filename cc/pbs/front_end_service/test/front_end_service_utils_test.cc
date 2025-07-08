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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/util/json_util.h>

#include "absl/status/status_matchers.h"
#include "cc/core/common/uuid/src/error_codes.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/core/test/utils/proto_test_utils.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/front_end_service/src/front_end_utils.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "proto/pbs/api/v1/api.pb.h"

namespace privacy_sandbox::pbs {
namespace {

using ::absl_testing::IsOk;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::JsonStringToMessage;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetResponse;
using ::privacy_sandbox::pbs_common::Byte;
using ::privacy_sandbox::pbs_common::BytesBuffer;
using ::privacy_sandbox::pbs_common::EqualsProto;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::kClaimedIdentityHeader;
using ::privacy_sandbox::pbs_common::ResultIs;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
using ::privacy_sandbox::pbs_common::Uuid;
using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

constexpr absl::string_view kAuthorizedDomain = "https://fake.com";

// Mock class with mock method to test ParseCommonV2TransactionRequestBody
class MockKeyBodyProcessor {
 public:
  MOCK_METHOD(ExecutionResult, ProcessKeyBody,
              (const privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest::
                   PrivacyBudgetKey&,
               size_t, absl::string_view));
};

TEST(FrontEndUtilsTest, ExtractTransactionId) {
  auto headers = std::make_shared<HttpHeaders>();
  Uuid transaction_id;
  EXPECT_EQ(ExtractTransactionIdFromHTTPHeaders(headers, transaction_id),
            FailureExecutionResult(
                SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  headers->insert(
      {std::string(kTransactionIdHeader), std::string("Asdasdasd")});
  EXPECT_EQ(ExtractTransactionIdFromHTTPHeaders(headers, transaction_id),
            FailureExecutionResult(pbs_common::SC_UUID_INVALID_STRING));

  headers->clear();
  headers->insert({std::string(kTransactionIdHeader),
                   std::string("3E2A3D09-48ED-A355-D346-AD7DC6CB0909")});
  EXPECT_EQ(ExtractTransactionIdFromHTTPHeaders(headers, transaction_id),
            SuccessExecutionResult());
}

TEST(FrontEndUtilsTest, ExtractTransactionOrigin) {
  HttpHeaders headers{};
  auto transaction_origin = ExtractTransactionOrigin(headers);
  EXPECT_EQ(transaction_origin.result(),
            FailureExecutionResult(
                SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  std::string extracted_transaction_origin;
  headers.insert({std::string(kTransactionOriginHeader),
                  std::string("This is the origin")});
  transaction_origin = ExtractTransactionOrigin(headers);
  EXPECT_THAT(transaction_origin.result(), SuccessExecutionResult());
  EXPECT_EQ(*transaction_origin, std::string("This is the origin"));
}

TEST(FrontEndUtilsTest, ExtractRequestClaimedIdentity) {
  std::shared_ptr<HttpHeaders> headers = nullptr;
  std::string claimed_identity;
  EXPECT_EQ(ExtractRequestClaimedIdentity(headers, claimed_identity),
            FailureExecutionResult(
                SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));
  headers = std::make_shared<HttpHeaders>();
  EXPECT_EQ(ExtractRequestClaimedIdentity(headers, claimed_identity),
            FailureExecutionResult(
                SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  std::string extracted_claimed_identity;
  headers->insert(
      {std::string(kClaimedIdentityHeader), std::string("other-coordinator")});
  EXPECT_EQ(ExtractRequestClaimedIdentity(headers, extracted_claimed_identity),
            SuccessExecutionResult());

  EXPECT_EQ(extracted_claimed_identity, std::string("other-coordinator"));
}

TEST(FrontEndUtilsTest, SerializeTransactionEmptyFailedCommandIndicesResponse) {
  std::vector<size_t> failed_indices;
  BytesBuffer bytes_buffer;

  EXPECT_EQ(SerializeTransactionFailedCommandIndicesResponse(failed_indices,
                                                             bytes_buffer),
            SuccessExecutionResult());

  std::string serialized_failed_response(bytes_buffer.bytes->begin(),
                                         bytes_buffer.bytes->end());
  ConsumePrivacyBudgetResponse received_response_proto;
  ASSERT_THAT(
      JsonStringToMessage(serialized_failed_response, &received_response_proto),
      IsOk());

  ConsumePrivacyBudgetResponse expected_response_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb(version: "1.0")pb",
                                          &expected_response_proto));
  EXPECT_THAT(received_response_proto, EqualsProto(expected_response_proto));
}

TEST(FrontEndUtilsTest, SerializeTransactionFailedCommandIndicesResponse) {
  std::vector<size_t> failed_indices = {1, 2, 3, 4, 5};
  BytesBuffer bytes_buffer;

  EXPECT_EQ(SerializeTransactionFailedCommandIndicesResponse(failed_indices,
                                                             bytes_buffer),
            SuccessExecutionResult());
  std::string serialized_failed_response(bytes_buffer.bytes->begin(),
                                         bytes_buffer.bytes->end());

  ConsumePrivacyBudgetResponse received_response_proto;
  ASSERT_THAT(
      JsonStringToMessage(serialized_failed_response, &received_response_proto),
      IsOk());

  ConsumePrivacyBudgetResponse expected_response_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(version: "1.0"
           exhausted_budget_indices: [ 1, 2, 3, 4, 5 ])pb",
      &expected_response_proto));
  EXPECT_THAT(received_response_proto, EqualsProto(expected_response_proto));
}

TEST(TransformReportingOriginToSite, Success) {
  auto site = TransformReportingOriginToSite("https://analytics.google.com");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpReportingOriginSuccess) {
  auto site = TransformReportingOriginToSite("http://analytics.google.com");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpReportingOriginWithPortSuccess) {
  auto site =
      TransformReportingOriginToSite("http://analytics.google.com:8080");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpReportingOriginWithSlashSuccess) {
  auto site = TransformReportingOriginToSite("http://analytics.google.com/");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite,
     HttpReportingOriginWithPortAndSlashSuccess) {
  auto site =
      TransformReportingOriginToSite("http://analytics.google.com:8080/");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, WithoutHttpsSuccess) {
  auto site = TransformReportingOriginToSite("analytics.google.com");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, WithoutHttpsWithPortSuccess) {
  auto site = TransformReportingOriginToSite("analytics.google.com:8080");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, WithoutHttpsWithSlashSuccess) {
  auto site = TransformReportingOriginToSite("analytics.google.com/");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, WithoutHttpsWithPortAndSlashSuccess) {
  auto site = TransformReportingOriginToSite("analytics.google.com:8080/");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, SiteToSiteSuccess) {
  auto site = TransformReportingOriginToSite("https://google.com");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, SiteWithPortToSiteSuccess) {
  auto site = TransformReportingOriginToSite("https://google.com:8080");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, SiteWithSlashToSiteSuccess) {
  auto site = TransformReportingOriginToSite("https://google.com/");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, SiteWithPortAndSlashToSiteSuccess) {
  auto site = TransformReportingOriginToSite("https://google.com:8080/");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpSiteToSiteSuccess) {
  auto site = TransformReportingOriginToSite("http://google.com");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpSiteWithPortToSiteSuccess) {
  auto site = TransformReportingOriginToSite("http://google.com:8080");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpSiteWithSlashToSiteSuccess) {
  auto site = TransformReportingOriginToSite("http://google.com/");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpSiteWithPortAndSlashToSiteSuccess) {
  auto site = TransformReportingOriginToSite("http://google.com:8080/");
  EXPECT_THAT(site.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, InvalidSite) {
  auto site = TransformReportingOriginToSite("******");
  EXPECT_THAT(site.result(),
              ResultIs(FailureExecutionResult(

                  SC_PBS_FRONT_END_SERVICE_INVALID_REPORTING_ORIGIN)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, ValidRequestSuccess) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
          keys { key: "234" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
        }
        data {
          reporting_origin: "http://b.fake.com"
          keys { key: "234" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
        }
      )pb",
      &request_proto));

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;

  MockKeyBodyProcessor mock_processor;
  // Expected arguments for the first call.
  PrivacyBudgetKey expected_key_body1;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z"
      )pb",
      &expected_key_body1));
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  // Expected arguments for the second call.
  PrivacyBudgetKey expected_key_body2;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        key: "234" token: 1 reporting_time: "2019-12-11T07:20:50.52Z"
      )pb",
      &expected_key_body2));
  size_t expected_key_index2 = 1;
  std::string expected_reporting_origin2 = "http://a.fake.com";

  // Expected arguments for the third call.
  PrivacyBudgetKey expected_key_body3;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        key: "234" token: 1 reporting_time: "2019-12-12T07:20:50.52Z"
      )pb",
      &expected_key_body3));
  size_t expected_key_index3 = 2;
  std::string expected_reporting_origin3 = "http://b.fake.com";

  // Act
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_body1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_body2),
                                             Eq(expected_key_index2),
                                             Eq(expected_reporting_origin2)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_body3),
                                             Eq(expected_key_index3),
                                             Eq(expected_reporting_origin3)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult result_proto = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });
  EXPECT_THAT(result_proto, ResultIs(SuccessExecutionResult()));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestEmptyProto) {
  ConsumePrivacyBudgetRequest request_proto;
  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result;
  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithInvalidVersion) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "5.0"
      )pb",
      &request_proto));
  ExecutionResult execution_result;
  MockKeyBodyProcessor mock_processor;

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithoutData) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
      )pb",
      &request_proto));

  MockKeyBodyProcessor mock_processor;
  EXPECT_CALL(mock_processor, ProcessKeyBody(_, _, _)).Times(0);

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  auto execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithoutReportingOrigin) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
        }
        data {
          # reporting_origin is implicitly empty here
          keys { key: "456" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
        }
      )pb",
      &request_proto));

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result;

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  PrivacyBudgetKey expected_key_body1;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z"
      )pb",
      &expected_key_body1));
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_body1),
                                             Eq(0), Eq("http://a.fake.com")))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithoutKeys) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
        }
        data {
          reporting_origin: "http://b.fake.com"
          # 'keys' field is absent, which is treated as empty in proto
        }
      )pb",
      &request_proto));

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result;

  // Expected arguments for the first call.
  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  PrivacyBudgetKey expected_key_body1;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z"
      )pb",
      &expected_key_body1));
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_body1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  // In proto we cannot distinguish between the "keys" key is absent or has a
  // default value. So, we expect success here.
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithEmptyReportingOrigin) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: ""
          keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
        }
      )pb",
      &request_proto));

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest,
     RequestWithInvalidReportingOrigin) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "invalid"
          keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
        }
      )pb",
      &request_proto));

  MockKeyBodyProcessor mock_processor;
  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest,
     RequestWithUnauthorizedReportingOrigin) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
        }
        data {
          reporting_origin: "http://b.shoe.com"
          keys { key: "456" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
        }
      )pb",
      &request_proto));

  MockKeyBodyProcessor mock_processor;

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  PrivacyBudgetKey expected_key_proto1;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z"
      )pb",
      &expected_key_proto1));
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_REPORTING_ORIGIN_NOT_BELONG_TO_SITE)));
}

TEST(ParseCommonV2TransactionRequestBodyTest,
     RequestWithRepeatedReportingOrigin) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
        }
        data {
          reporting_origin: "http://a.fake.com"
          keys { key: "456" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
        }
      )pb",
      &request_proto));

  MockKeyBodyProcessor mock_processor;

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  PrivacyBudgetKey expected_key_proto1;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z"
      )pb",
      &expected_key_proto1));
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result, ResultIs(FailureExecutionResult(
                                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithBudgetTypeSpecified) {
  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys {
            key: "123"
            token: 1
            reporting_time: "2019-12-11T07:20:50.52Z"
            budget_type: BUDGET_TYPE_BINARY_BUDGET
          }
        }
        data {
          reporting_origin: "http://b.fake.com"
          keys {
            key: "456"
            token: 1
            reporting_time: "2019-12-12T07:20:50.52Z"
            budget_type: BUDGET_TYPE_BINARY_BUDGET
          }
        }
      )pb",
      &request_proto));

  MockKeyBodyProcessor mock_processor;

  PrivacyBudgetKey expected_key_proto1;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        key: "123"
        token: 1
        reporting_time: "2019-12-11T07:20:50.52Z"
        budget_type: BUDGET_TYPE_BINARY_BUDGET
      )pb",
      &expected_key_proto1));
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  PrivacyBudgetKey expected_key_proto2;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        key: "456"
        token: 1
        reporting_time: "2019-12-12T07:20:50.52Z"
        budget_type: BUDGET_TYPE_BINARY_BUDGET
      )pb",
      &expected_key_proto2));
  size_t expected_key_index2 = 1;
  std::string expected_reporting_origin2 = "http://b.fake.com";

  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto2),
                                             Eq(expected_key_index2),
                                             Eq(expected_reporting_origin2)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));
}

TEST(ParseCommonV2TransactionRequestBodyTest,
     RequestWithBudgetTypeNotSpecifiedInSecondKey) {
  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "https://a.fake.com"
          keys {
            key: "123"
            token: 1
            reporting_time: "2019-12-11T07:20:50.52Z"
            budget_type: BUDGET_TYPE_BINARY_BUDGET
          }
        }
        data {
          reporting_origin: "https://b.fake.com"
          keys { key: "456" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
        }
      )pb",
      &request_proto));

  MockKeyBodyProcessor mock_processor;
  EXPECT_CALL(mock_processor, ProcessKeyBody(_, _, _))
      .Times(2)
      .WillRepeatedly(Return(SuccessExecutionResult()));
  ExecutionResult execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithNoData) {
  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
      )pb",
      &request_proto));

  MockKeyBodyProcessor mock_processor;
  EXPECT_CALL(mock_processor, ProcessKeyBody(_, _, _)).Times(0);

  ExecutionResult execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));
}

TEST(CheckAndGetIfBudgetTypeTheSameInRequestTest,
     RequestWithNoBudgetTypeSpecified) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
        }
        data {
          reporting_origin: "http://b.fake.com"
          keys { key: "456" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
        }
      )pb",
      &request_proto));
  auto execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_THAT(execution_result_proto, ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(
      *execution_result_proto,
      ConsumePrivacyBudgetRequest::PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET);
}

TEST(CheckAndGetIfBudgetTypeTheSameInRequestTest,
     RequestWithSameBudgetTypeSpecified) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys {
            key: "123"
            token: 1
            reporting_time: "2019-12-11T07:20:50.52Z"
            budget_type: BUDGET_TYPE_BINARY_BUDGET
          }
        }
        data {
          reporting_origin: "http://b.fake.com"
          keys {
            key: "456"
            token: 1
            reporting_time: "2019-12-12T07:20:50.52Z"
            budget_type: BUDGET_TYPE_BINARY_BUDGET
          }
        }
      )pb",
      &request_proto));
  auto execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_THAT(execution_result_proto, ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(
      *execution_result_proto,
      ConsumePrivacyBudgetRequest::PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET);
}

TEST(CheckAndGetIfBudgetTypeTheSameInRequestTest,
     RequestWithDifferentBudgetTypeSpecified) {
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys { key: "123" token: 1 reporting_time: "2019-12-11T07:20:50.52Z" }
        }
        data {
          reporting_origin: "http://b.fake.com"
          keys { key: "456" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
        }
      )pb",
      &request_proto));

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;

  // This an attempt to introduce fake budget types since the only
  // available budget types available at this time are BUDGET_TYPE_UNSPECIFIED
  // and BUDGET_TYPE_BINARY_BUDGET which are equivalent
  PrivacyBudgetKey::BudgetType fake_budget_type1 =
      static_cast<typename PrivacyBudgetKey::BudgetType>(1000);
  PrivacyBudgetKey::BudgetType fake_budget_type2 =
      static_cast<typename PrivacyBudgetKey::BudgetType>(1001);
  request_proto.mutable_data(0)->mutable_keys(0)->set_budget_type(
      fake_budget_type1);
  request_proto.mutable_data(1)->mutable_keys(0)->set_budget_type(
      fake_budget_type2);

  auto execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_THAT(execution_result_proto,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

TEST(CheckAndGetIfBudgetTypeTheSameInRequestTest,
     RequestWithBudgetTypeNotSpecifiedInSecondKey) {
  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  ConsumePrivacyBudgetRequest request_proto;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        version: "2.0"
        data {
          reporting_origin: "http://a.fake.com"
          keys {
            key: "123"
            token: 1
            reporting_time: "2019-12-11T07:20:50.52Z"
            budget_type: BUDGET_TYPE_BINARY_BUDGET
          }
        }
        data {
          reporting_origin: "http://b.fake.com"
          keys { key: "456" token: 1 reporting_time: "2019-12-12T07:20:50.52Z" }
        }
      )pb",
      &request_proto));
  auto execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_THAT(execution_result_proto, ResultIs(SuccessExecutionResult()));
  EXPECT_EQ(*execution_result_proto,
            PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET);

  // This an attempt to introduce fake budget types since the only
  // available budget types available at this time are BUDGET_TYPE_UNSPECIFIED
  // and BUDGET_TYPE_BINARY_BUDGET which are equivalent
  PrivacyBudgetKey::BudgetType fake_budget_type =
      static_cast<typename PrivacyBudgetKey::BudgetType>(1000);
  request_proto.mutable_data(0)->mutable_keys(0)->set_budget_type(
      fake_budget_type);

  execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_THAT(execution_result_proto,
              ResultIs(FailureExecutionResult(
                  SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

}  // namespace
}  // namespace privacy_sandbox::pbs
