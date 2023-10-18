
// Copyright 2022 Google LLC
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

#include "cpio/client_providers/private_key_fetcher_provider/src/gcp/gcp_private_key_fetcher_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "absl/strings/str_cat.h"
#include "core/http2_client/mock/mock_http_client.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/auth_token_provider/mock/mock_auth_token_provider.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/error_codes.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/gcp/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_GCP_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::GcpPrivateKeyFetcherProvider;
using google::scp::cpio::client_providers::mock::MockAuthTokenProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;
using testing::UnorderedElementsAre;

namespace {
constexpr char kAccountIdentity[] = "accountIdentity";
constexpr char kRegion[] = "us-east-1";
constexpr char kKeyId[] = "123";
constexpr char kPrivateKeyBaseUri[] = "http://localhost.test:8000";
constexpr char kPrivateKeyCloudfunctionUri[] = "http://cloudfunction.test:8000";
constexpr char kSessionTokenMock[] = "session-token-test";
constexpr char kAuthorizationHeaderKey[] = "Authorization";
constexpr char kBearerTokenPrefix[] = "Bearer ";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class GcpPrivateKeyFetcherProviderTest : public ::testing::Test {
 protected:
  GcpPrivateKeyFetcherProviderTest()
      : http_client_(make_shared<MockHttpClient>()),
        credentials_provider_(make_shared<MockAuthTokenProvider>()),
        gcp_private_key_fetcher_provider_(
            make_unique<GcpPrivateKeyFetcherProvider>(http_client_,
                                                      credentials_provider_)) {
    EXPECT_SUCCESS(gcp_private_key_fetcher_provider_->Init());
    EXPECT_SUCCESS(gcp_private_key_fetcher_provider_->Run());

    request_ = make_shared<PrivateKeyFetchingRequest>();
    request_->key_id = make_shared<string>(kKeyId);
    auto endpoint = make_shared<PrivateKeyVendingEndpoint>();
    endpoint->gcp_private_key_vending_service_cloudfunction_url =
        kPrivateKeyCloudfunctionUri;
    endpoint->private_key_vending_service_endpoint = kPrivateKeyBaseUri;
    endpoint->service_region = kRegion;
    endpoint->account_identity = kAccountIdentity;
    request_->key_vending_endpoint = move(endpoint);
  }

  ~GcpPrivateKeyFetcherProviderTest() {
    if (gcp_private_key_fetcher_provider_) {
      EXPECT_SUCCESS(gcp_private_key_fetcher_provider_->Stop());
    }
  }

  void MockRequest(const string& uri) {
    http_client_->request_mock = HttpRequest();
    http_client_->request_mock.path = make_shared<string>(uri);
  }

  void MockResponse(const string& str) {
    http_client_->response_mock = HttpResponse();
    http_client_->response_mock.body = BytesBuffer(str);
  }

  shared_ptr<MockHttpClient> http_client_;
  shared_ptr<MockAuthTokenProvider> credentials_provider_;
  unique_ptr<GcpPrivateKeyFetcherProvider> gcp_private_key_fetcher_provider_;
  shared_ptr<PrivateKeyFetchingRequest> request_;
};

TEST_F(GcpPrivateKeyFetcherProviderTest, MissingHttpClient) {
  gcp_private_key_fetcher_provider_ =
      make_unique<GcpPrivateKeyFetcherProvider>(nullptr, credentials_provider_);

  EXPECT_THAT(gcp_private_key_fetcher_provider_->Init(),
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND)));
}

TEST_F(GcpPrivateKeyFetcherProviderTest, MissingCredentialsProvider) {
  gcp_private_key_fetcher_provider_ =
      make_unique<GcpPrivateKeyFetcherProvider>(http_client_, nullptr);

  EXPECT_THAT(
      gcp_private_key_fetcher_provider_->Init(),
      ResultIs(FailureExecutionResult(
          SC_GCP_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND)));
}

MATCHER_P(TargetAudienceUriEquals, expected_target_audience_uri, "") {
  return ExplainMatchResult(*arg.request->token_target_audience_uri,
                            expected_target_audience_uri, result_listener);
}

TEST_F(GcpPrivateKeyFetcherProviderTest, SignHttpRequest) {
  atomic<bool> condition = false;

  EXPECT_CALL(*credentials_provider_,
              GetSessionTokenForTargetAudience(
                  TargetAudienceUriEquals(kPrivateKeyCloudfunctionUri)))
      .WillOnce([=](AsyncContext<GetSessionTokenForTargetAudienceRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  AsyncContext<PrivateKeyFetchingRequest, HttpRequest> context(
      request_,
      [&](AsyncContext<PrivateKeyFetchingRequest, HttpRequest>& context) {
        EXPECT_SUCCESS(context.result);
        const auto& signed_request_ = *context.response;

        EXPECT_EQ(signed_request_.method, HttpMethod::GET);
        EXPECT_THAT(signed_request_.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        condition = true;
        return SuccessExecutionResult();
      });

  EXPECT_THAT(gcp_private_key_fetcher_provider_->SignHttpRequest(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpPrivateKeyFetcherProviderTest, FailedToGetCredentials) {
  EXPECT_CALL(*credentials_provider_,
              GetSessionTokenForTargetAudience(
                  TargetAudienceUriEquals(kPrivateKeyCloudfunctionUri)))
      .WillOnce([=](AsyncContext<GetSessionTokenForTargetAudienceRequest,
                                 GetSessionTokenResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return context.result;
      });

  atomic<bool> condition = false;
  AsyncContext<PrivateKeyFetchingRequest, HttpRequest> context(
      request_,
      [&](AsyncContext<PrivateKeyFetchingRequest, HttpRequest>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        condition = true;
      });

  EXPECT_THAT(gcp_private_key_fetcher_provider_->SignHttpRequest(context),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
