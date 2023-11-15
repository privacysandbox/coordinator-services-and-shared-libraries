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

#include "pbs/authorization/src/aws/aws_http_request_response_auth_interceptor.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest-matchers.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>
#include <gtest/gtest_pred_impl.h>

#include <array>
#include <initializer_list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "core/authorization_service/src/error_codes.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/http2_client/src/aws/aws_v4_signer.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/type_def.h"
#include "core/utils/src/base64.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

namespace google::scp::pbs {
namespace {

using ::google::scp::core::AuthorizationMetadata;
using ::google::scp::core::AuthorizedMetadata;
using ::google::scp::core::AwsV4Signer;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::HttpHeaders;
using ::google::scp::core::HttpRequest;
using ::google::scp::core::HttpResponse;
using ::google::scp::core::RetryExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::config_provider::mock::MockConfigProvider;
using ::google::scp::core::test::IsSuccessful;
using ::google::scp::core::test::IsSuccessfulAndHolds;
using ::google::scp::core::test::ResultIs;
using ::google::scp::core::utils::Base64Encode;
using ::std::make_pair;
using ::std::make_shared;
using ::std::shared_ptr;
using ::std::string;
using ::testing::ContainsRegex;
using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::Pointee;

using json = nlohmann::json;

constexpr char kRegion[] = "us-east-1";
constexpr char kIdentity[] = "identity";

constexpr char kAccessKey[] = "access_key";
constexpr char kSignature[] = "signature";
constexpr char kAmzDate[] = "amz_date";
constexpr char kSecurityToken[] = "security_token";
constexpr std::array kSignedHeaders = {"host", "x-amz-date"};

constexpr char kAuthorizationHeader[] = "Authorization";

class AwsHttpRequestResponseAuthInterceptorTest : public testing::Test {
 protected:
  AwsHttpRequestResponseAuthInterceptorTest() : subject_(kRegion) {
    token_json_ = json::parse(R"""({
        "access_key": "accesskey",
        "signature": "signature",
        "amz_date": "amzdate"
      })""");
    if (!Base64Encode(token_json_.dump(),
                      authorization_metadata_.authorization_token)
             .Successful()) {
      throw std::runtime_error("error encoding");
    }
    authorization_metadata_.claimed_identity = kIdentity;

    http_request_.headers = make_shared<HttpHeaders>();
  }

  json token_json_;

  AwsHttpRequestResponseAuthInterceptor subject_;

  AuthorizationMetadata authorization_metadata_;
  HttpRequest http_request_;
};

TEST_F(AwsHttpRequestResponseAuthInterceptorTest, PrepareRequest) {
  EXPECT_THAT(subject_.PrepareRequest(authorization_metadata_, http_request_),
              IsSuccessful());

  const auto& headers = *http_request_.headers;

  ASSERT_NE(headers.find(core::kClaimedIdentityHeader), headers.end());
  ASSERT_EQ(headers.find(core::kClaimedIdentityHeader)->second, kIdentity);

  ASSERT_NE(headers.find(kAuthorizationHeader), headers.end());
  EXPECT_THAT(headers.find(kAuthorizationHeader)->second,
              ContainsRegex(absl::StrCat(
                  "SignedHeaders=", absl::StrJoin(kSignedHeaders, ";"), ".*",
                  "signature")));
  EXPECT_EQ(headers.find(core::kEnablePerSiteEnrollmentHeader), headers.end());
}

TEST_F(AwsHttpRequestResponseAuthInterceptorTest,
       PrepareRequestWithConfigProvider) {
  auto mock_config_provider = std::make_shared<MockConfigProvider>();
  subject_ =
      AwsHttpRequestResponseAuthInterceptor(kRegion, mock_config_provider);
  EXPECT_THAT(subject_.PrepareRequest(authorization_metadata_, http_request_),
              IsSuccessful());

  const auto& headers = *http_request_.headers;

  ASSERT_NE(headers.find(core::kClaimedIdentityHeader), headers.end());
  ASSERT_EQ(headers.find(core::kClaimedIdentityHeader)->second, kIdentity);

  ASSERT_NE(headers.find(kAuthorizationHeader), headers.end());
  EXPECT_THAT(headers.find(kAuthorizationHeader)->second,
              ContainsRegex(absl::StrCat(
                  "SignedHeaders=", absl::StrJoin(kSignedHeaders, ";"), ".*",
                  "signature")));
  EXPECT_EQ(headers.find(core::kEnablePerSiteEnrollmentHeader), headers.end());
}

TEST_F(AwsHttpRequestResponseAuthInterceptorTest,
       PrepareRequestEnablePerSiteEnrollment) {
  auto mock_config_provider = std::make_shared<MockConfigProvider>();
  mock_config_provider->SetBool(
      core::kPBSAuthorizationEnableSiteBasedAuthorization, true);
  subject_ =
      AwsHttpRequestResponseAuthInterceptor(kRegion, mock_config_provider);
  EXPECT_THAT(subject_.PrepareRequest(authorization_metadata_, http_request_),
              IsSuccessful());

  const auto& headers = *http_request_.headers;

  ASSERT_NE(headers.find(core::kClaimedIdentityHeader), headers.end());
  ASSERT_EQ(headers.find(core::kClaimedIdentityHeader)->second, kIdentity);

  ASSERT_NE(headers.find(kAuthorizationHeader), headers.end());
  EXPECT_THAT(headers.find(kAuthorizationHeader)->second,
              ContainsRegex(absl::StrCat(
                  "SignedHeaders=", absl::StrJoin(kSignedHeaders, ";"), ".*",
                  "signature")));
  ASSERT_NE(headers.find(core::kEnablePerSiteEnrollmentHeader), headers.end());
  EXPECT_EQ(headers.find(core::kEnablePerSiteEnrollmentHeader)->second, "true");
}

TEST_F(AwsHttpRequestResponseAuthInterceptorTest,
       PrepareRequestWithSecurityToken) {
  token_json_[kSecurityToken] = "securitytoken";
  ASSERT_THAT(Base64Encode(token_json_.dump(),
                           authorization_metadata_.authorization_token),
              IsSuccessful());
  EXPECT_THAT(subject_.PrepareRequest(authorization_metadata_, http_request_),
              IsSuccessful());

  const auto& headers = *http_request_.headers;

  ASSERT_NE(headers.find(core::kClaimedIdentityHeader), headers.end());
  ASSERT_EQ(headers.find(core::kClaimedIdentityHeader)->second, kIdentity);

  ASSERT_NE(headers.find(kAuthorizationHeader), headers.end());
  EXPECT_THAT(headers.find(kAuthorizationHeader)->second,
              ContainsRegex(absl::StrCat(
                  "SignedHeaders=", absl::StrJoin(kSignedHeaders, ";"), ".*",
                  "signature")));
}

TEST_F(AwsHttpRequestResponseAuthInterceptorTest,
       PrepareRequestFailsIfBadMetadata) {
  AuthorizationMetadata bad_metadata;
  EXPECT_THAT(subject_.PrepareRequest(bad_metadata, http_request_),
              ResultIs(core::FailureExecutionResult(
                  core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN)));

  bad_metadata.authorization_token = "some_token";
  EXPECT_THAT(subject_.PrepareRequest(bad_metadata, http_request_),
              ResultIs(core::FailureExecutionResult(
                  core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN)));
}

TEST_F(AwsHttpRequestResponseAuthInterceptorTest,
       PrepareRequestFailsIfBadJson) {
  for (const auto& key : {kAccessKey, kSignature, kAmzDate}) {
    json incomplete_json = token_json_;
    incomplete_json.erase(key);
    EXPECT_THAT(Base64Encode(incomplete_json.dump(),
                             authorization_metadata_.authorization_token),
                IsSuccessful());
    EXPECT_THAT(subject_.PrepareRequest(authorization_metadata_, http_request_),
                ResultIs(core::FailureExecutionResult(
                    core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN)));
  }

  authorization_metadata_.authorization_token = "bad_json";
  EXPECT_THAT(subject_.PrepareRequest(authorization_metadata_, http_request_),
              ResultIs(core::FailureExecutionResult(
                  core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN)));
}

TEST_F(AwsHttpRequestResponseAuthInterceptorTest, ObtainAuthorizedMetadata) {
  json response_json = json::parse(R"""({
      "authorized_domain": "domain"
    })""");
  auto response_json_str = response_json.dump();

  AuthorizationMetadata request_auth_metadata;
  HttpResponse http_response;
  http_response.body = core::BytesBuffer(response_json_str);
  EXPECT_THAT(subject_.ObtainAuthorizedMetadataFromResponse(
                  request_auth_metadata, http_response),
              IsSuccessfulAndHolds(FieldsAre(Pointee(Eq("domain")))));
}

TEST_F(AwsHttpRequestResponseAuthInterceptorTest,
       ObtainAuthorizedMetadataFailsIfBadJson) {
  json response_json = json::parse(R"""({})""");
  auto response_json_str = response_json.dump();

  AuthorizationMetadata request_auth_metadata;
  HttpResponse http_response;
  http_response.body = core::BytesBuffer(response_json_str);
  EXPECT_THAT(subject_.ObtainAuthorizedMetadataFromResponse(
                  request_auth_metadata, http_response),
              ResultIs(core::FailureExecutionResult(
                  core::errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN)));
}

}  // namespace
}  // namespace google::scp::pbs
