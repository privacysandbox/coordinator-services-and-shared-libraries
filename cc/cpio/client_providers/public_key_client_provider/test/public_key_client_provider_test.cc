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

#include "cpio/client_providers/public_key_client_provider/src/public_key_client_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "core/http2_client/mock/mock_http_client.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_ALL_URIS_REQUEST_PERFORM_FAILED;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_HTTP_CLIENT_REQUIRED;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_INVALID_CONFIG_OPTIONS;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;

static constexpr char kPublicKeyHeaderDate[] = "date";
static constexpr char kPublicKeyHeaderCacheControl[] = "cache-control";
static constexpr char kPrivateKeyBaseUri1[] = "http://public_key/publicKeys1";
static constexpr char kPrivateKeyBaseUri2[] = "http://public_key/publicKeys2";
static constexpr char kHeaderDateExample[] = "Wed, 16 Nov 2022 00:02:48 GMT";
static constexpr char kCacheControlExample[] = "max-age=254838";
static constexpr uint64_t kExpectedExpiredTimeInSeconds = 1668811806;

namespace google::scp::cpio::client_providers::test {

TEST(PublicKeyClientProviderTestI, InitFailedWithInvalidConfig) {
  auto http_client = make_shared<MockHttpClient>();

  auto public_key_client_options = make_shared<PublicKeyClientOptions>();

  auto public_key_client = make_unique<PublicKeyClientProvider>(
      public_key_client_options, http_client);

  EXPECT_THAT(public_key_client->Init(),
              ResultIs(FailureExecutionResult(
                  SC_PUBLIC_KEY_CLIENT_PROVIDER_INVALID_CONFIG_OPTIONS)));
}

TEST(PublicKeyClientProviderTestI, InitFailedInvalidHttpClient) {
  auto public_key_client_options = make_shared<PublicKeyClientOptions>();
  public_key_client_options->endpoints.emplace_back(kPrivateKeyBaseUri1);

  auto public_key_client =
      make_unique<PublicKeyClientProvider>(public_key_client_options, nullptr);

  EXPECT_THAT(public_key_client->Init(),
              ResultIs(FailureExecutionResult(
                  SC_PUBLIC_KEY_CLIENT_PROVIDER_HTTP_CLIENT_REQUIRED)));
}

class PublicKeyClientProviderTestII : public ::testing::Test {
 protected:
  void SetUp() override {
    http_client_ = make_shared<MockHttpClient>();

    auto public_key_client_options = make_shared<PublicKeyClientOptions>();
    public_key_client_options->endpoints.emplace_back(kPrivateKeyBaseUri1);
    public_key_client_options->endpoints.emplace_back(kPrivateKeyBaseUri2);

    public_key_client_ = make_unique<PublicKeyClientProvider>(
        public_key_client_options, http_client_);

    EXPECT_SUCCESS(public_key_client_->Init());
    EXPECT_SUCCESS(public_key_client_->Run());
  }

  HttpResponse GetValidHttpResponse() {
    HttpResponse response;
    HttpHeaders headers;
    headers.insert({kPublicKeyHeaderDate, kHeaderDateExample});
    headers.insert({kPublicKeyHeaderCacheControl, kCacheControlExample});
    response.headers = make_shared<HttpHeaders>(headers);

    string bytes_str =
        "{\"keys\": [{\"id\": \"1234\", \"key\": \"abcdefg\"}, "
        "{\"id\": \"5678\", \"key\": \"hijklmn\"}]}";
    BytesBuffer bytes(bytes_str.length());

    response.body.bytes->assign(bytes_str.begin(), bytes_str.end());
    response.body.length = bytes_str.length();
    return response;
  }

  void TearDown() override {
    if (public_key_client_) {
      EXPECT_SUCCESS(public_key_client_->Stop());
    }
  }

  shared_ptr<MockHttpClient> http_client_;
  unique_ptr<PublicKeyClientProvider> public_key_client_;
};

TEST_F(PublicKeyClientProviderTestII, ListPublicKeysSuccess) {
  atomic<int> perform_calls(0);
  auto success_response = GetValidHttpResponse();
  http_client_->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        perform_calls++;
        http_context.response = make_shared<HttpResponse>(success_response);
        http_context.result = SuccessExecutionResult();
        http_context.Finish();
        return SuccessExecutionResult();
      };

  auto request = make_shared<ListPublicKeysRequest>();

  atomic<int> success_callback(0);
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      move(request), [&](AsyncContext<ListPublicKeysRequest,
                                      ListPublicKeysResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->public_keys()[0].key_id(), "1234");
        EXPECT_EQ(context.response->public_keys()[0].public_key(), "abcdefg");
        EXPECT_EQ(context.response->public_keys()[1].key_id(), "5678");
        EXPECT_EQ(context.response->public_keys()[1].public_key(), "hijklmn");
        EXPECT_EQ(context.response->expiration_time().seconds(),
                  kExpectedExpiredTimeInSeconds);
        success_callback++;
      });

  EXPECT_SUCCESS(public_key_client_->ListPublicKeys(context));
  // ListPublicKeys context callback will only run once even all uri get
  // success.
  WaitUntil([&]() { return success_callback.load() == 1; });
  // Http client PerformRequest() has being called twice. All uris in
  // public_key_client_options will being called.
  WaitUntil([&]() { return perform_calls.load() == 2; });
}

TEST_F(PublicKeyClientProviderTestII, ListPublicKeysFailure) {
  ExecutionResult failed_result = FailureExecutionResult(SC_UNKNOWN);

  atomic<int> perform_calls(0);
  auto success_response = GetValidHttpResponse();
  http_client_->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        perform_calls++;

        http_context.result = failed_result;
        http_context.Finish();
        return SuccessExecutionResult();
      };

  auto request = make_shared<ListPublicKeysRequest>();

  atomic<int> failure_callback(0);
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      move(request), [&](AsyncContext<ListPublicKeysRequest,
                                      ListPublicKeysResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        failure_callback++;
      });

  EXPECT_SUCCESS(public_key_client_->ListPublicKeys(context));

  // ListPublicKeys context callback will only run once even all uri get
  // fail.
  WaitUntil([&]() { return failure_callback.load() == 1; });
  WaitUntil([&]() { return perform_calls.load() == 2; });
}

TEST_F(PublicKeyClientProviderTestII, AllUrisPerformRequestFailed) {
  atomic<int> perform_calls(0);
  auto success_response = GetValidHttpResponse();
  http_client_->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        perform_calls++;
        return FailureExecutionResult(SC_UNKNOWN);
      };

  auto request = make_shared<ListPublicKeysRequest>();

  auto cpio_failure = FailureExecutionResult(
      SC_PUBLIC_KEY_CLIENT_PROVIDER_ALL_URIS_REQUEST_PERFORM_FAILED);

  atomic<int> failure_callback(0);
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      move(request), [&](AsyncContext<ListPublicKeysRequest,
                                      ListPublicKeysResponse>& context) {
        EXPECT_THAT(context.result, ResultIs(cpio_failure));
        failure_callback++;
      });

  EXPECT_THAT(public_key_client_->ListPublicKeys(context),
              ResultIs(cpio_failure));

  // ListPublicKeys context callback will only run once even all uri get
  // fail.
  WaitUntil([&]() { return failure_callback.load() == 1; });
  WaitUntil([&]() { return perform_calls.load() == 2; });
}

TEST_F(PublicKeyClientProviderTestII, ListPublicKeysPartialUriSuccess) {
  ExecutionResult failed_result = FailureExecutionResult(SC_UNKNOWN);
  atomic<int> perform_calls(0);
  auto success_response = GetValidHttpResponse();
  http_client_->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        perform_calls++;
        if (*http_context.request->path == kPrivateKeyBaseUri2) {
          http_context.response = make_shared<HttpResponse>(success_response);
          http_context.result = SuccessExecutionResult();
          http_context.Finish();
          return SuccessExecutionResult();
        }

        http_context.result = failed_result;
        http_context.Finish();

        return failed_result;
      };

  auto request = make_shared<ListPublicKeysRequest>();
  atomic<int> success_callback(0);
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      move(request), [&](AsyncContext<ListPublicKeysRequest,
                                      ListPublicKeysResponse>& context) {
        EXPECT_SUCCESS(context.result);
        success_callback++;
      });

  EXPECT_SUCCESS(public_key_client_->ListPublicKeys(context));
  // ListPublicKeys success with partial uris got success response.
  WaitUntil([&]() { return success_callback.load() == 1; });
  WaitUntil([&]() { return perform_calls.load() == 2; });
}

}  // namespace google::scp::cpio::client_providers::test
