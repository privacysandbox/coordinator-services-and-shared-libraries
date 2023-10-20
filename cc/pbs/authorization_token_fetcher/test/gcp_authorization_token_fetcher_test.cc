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

#include "pbs/authorization_token_fetcher/src/gcp/gcp_authorization_token_fetcher.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "core/async_executor/src/async_executor.h"
#include "core/authorization_service/src/error_codes.h"
#include "core/curl_client/mock/mock_curl_client.h"
#include "core/test/utils/conditional_wait.h"
#include "core/utils/src/base64.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::pbs::GcpAuthorizationTokenFetcher;
using std::atomic_bool;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_ptr;
using std::chrono::seconds;
using testing::EndsWith;
using testing::Eq;
using testing::Pair;
using testing::Pointee;
using testing::UnorderedElementsAre;

namespace google::scp::core::test {
namespace {

constexpr char kTokenServerPath[] =
    "http://metadata/computeMetadata/v1/instance/service-accounts/default/"
    "identity";
constexpr char kMetadataFlavorHeader[] = "Metadata-Flavor";
constexpr char kMetadataFlavorHeaderValue[] = "Google";

constexpr char kAudience[] = "www.google.com";

// eyJleHAiOjE2NzI3NjA3MDEsImlzcyI6Imlzc3VlciIsImF1ZCI6ImF1ZGllbmNlIiwic3ViIjoic3ViamVjdCIsImlhdCI6MTY3Mjc1NzEwMX0=
// decodes to:
// "{"exp":1672760701,"iss":"issuer","aud":"audience","sub":"subject","iat":1672757101}"
constexpr char kBase64EncodedResponse[] =
    "someheader."
    "eyJleHAiOjE2NzI3NjA3MDEsImlzcyI6Imlzc3VlciIsImF1ZCI6ImF1ZGllbmNlIiwic3ViIj"
    "oic3ViamVjdCIsImlhdCI6MTY3Mjc1NzEwMX0=.signature";
constexpr seconds kTokenLifetime = seconds(3600);

class GcpAuthorizationTokenFetcherTest : public testing::TestWithParam<string> {
 protected:
  GcpAuthorizationTokenFetcherTest()
      : http_client_(make_shared<MockCurlClient>()),
        async_executor_(make_shared<AsyncExecutor>(2, 20)) {
    async_executor_->Init();
    async_executor_->Run();

    subject_ = make_unique<GcpAuthorizationTokenFetcher>(
        http_client_, kAudience, async_executor_);
  }

  ~GcpAuthorizationTokenFetcherTest() { async_executor_->Stop(); }

  string GetResponseBody() { return GetParam(); }

  shared_ptr<MockCurlClient> http_client_;

  AsyncContext<FetchTokenRequest, FetchTokenResponse> fetch_token_context_;

  shared_ptr<AsyncExecutorInterface> async_executor_;
  unique_ptr<GcpAuthorizationTokenFetcher> subject_;
};

TEST_F(GcpAuthorizationTokenFetcherTest,
       FetchTokenGivesValidTokenAndTokenExpirationTimestamp) {
  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = SuccessExecutionResult();
    EXPECT_EQ(http_context.request->method, HttpMethod::GET);
    EXPECT_THAT(http_context.request->path, Pointee(Eq(kTokenServerPath)));
    EXPECT_THAT(http_context.request->query,
                Pointee(absl::StrCat("audience=", kAudience, "&format=full")));
    EXPECT_THAT(http_context.request->headers,
                Pointee(UnorderedElementsAre(
                    Pair(kMetadataFlavorHeader, kMetadataFlavorHeaderValue))));

    http_context.response = make_shared<HttpResponse>();
    http_context.response->body = BytesBuffer(kBase64EncodedResponse);
    http_context.Finish();
    return SuccessExecutionResult();
  });

  atomic_bool finished(false);
  fetch_token_context_.callback = [&finished](auto& context) {
    EXPECT_THAT(context.result, IsSuccessful());
    if (!context.response) {
      ADD_FAILURE();
    } else {
      EXPECT_EQ(context.response->token, kBase64EncodedResponse);
      EXPECT_EQ(context.response->token_lifetime_in_seconds, kTokenLifetime);
    }
    finished = true;
  };
  EXPECT_THAT(subject_->FetchToken(fetch_token_context_), IsSuccessful());

  WaitUntil([&finished]() { return finished.load(); });
}

TEST_F(GcpAuthorizationTokenFetcherTest, FetchTokenFailsIfHttpRequestFails) {
  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result =
        FailureExecutionResult(errors::SC_AUTHORIZATION_SERVICE_INTERNAL_ERROR);
    http_context.Finish();
    return SuccessExecutionResult();
  });

  atomic_bool finished(false);
  fetch_token_context_.callback = [&finished](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    errors::SC_AUTHORIZATION_SERVICE_INTERNAL_ERROR)));
    finished = true;
  };
  EXPECT_THAT(subject_->FetchToken(fetch_token_context_), IsSuccessful());

  WaitUntil([&finished]() { return finished.load(); });
}

TEST_P(GcpAuthorizationTokenFetcherTest, FetchTokenFailsIfBadJson) {
  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([this](auto& http_context) {
        http_context.result = SuccessExecutionResult();

        http_context.response = make_shared<HttpResponse>();
        http_context.response->body = BytesBuffer(GetResponseBody());
        http_context.Finish();
        return SuccessExecutionResult();
      });

  std::atomic_bool finished(false);
  fetch_token_context_.callback = [&finished](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(RetryExecutionResult(
                    errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN)));
    finished = true;
  };
  EXPECT_THAT(subject_->FetchToken(fetch_token_context_), IsSuccessful());

  WaitUntil([&finished]() { return finished.load(); });
}

INSTANTIATE_TEST_SUITE_P(
    BadTokens, GcpAuthorizationTokenFetcherTest,
    testing::Values("onlytwo.parts",
                    // decodes to "{"field":}" which is a malformed json.
                    "header.eyJmaWVsZCI6fQ==.signature",
                    // decodes to "{}", i.e. an empty json object.
                    "header.e30=.signature"));

}  // namespace
}  // namespace google::scp::core::test
