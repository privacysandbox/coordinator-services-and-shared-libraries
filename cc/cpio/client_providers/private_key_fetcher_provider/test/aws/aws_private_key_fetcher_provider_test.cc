
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

#include "cpio/client_providers/private_key_fetcher_provider/src/aws/aws_private_key_fetcher_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include <aws/core/Aws.h>

#include "core/http2_client/mock/mock_http_client.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/aws/error_codes.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/error_codes.h"
#include "cpio/client_providers/role_credentials_provider/mock/mock_role_credentials_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::scp::core::AsyncContext;
using google::scp::core::AwsV4Signer;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::AwsPrivateKeyFetcherProvider;
using google::scp::cpio::client_providers::mock::MockRoleCredentialsProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

namespace {
constexpr char kAccountIdentity[] = "accountIdentity";
constexpr char kRegion[] = "us-east-1";
constexpr char kKeyId[] = "123";
constexpr char kPrivateKeyBaseUri[] = "http://localhost.test:8000";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class AwsPrivateKeyFetcherProviderTest : public ::testing::Test {
 protected:
  AwsPrivateKeyFetcherProviderTest()
      : http_client_(make_shared<MockHttpClient>()),
        credentials_provider_(make_shared<MockRoleCredentialsProvider>()),
        aws_private_key_fetcher_provider_(
            make_unique<AwsPrivateKeyFetcherProvider>(http_client_,
                                                      credentials_provider_)) {
    SDKOptions options;
    InitAPI(options);
    EXPECT_SUCCESS(aws_private_key_fetcher_provider_->Init());
    EXPECT_SUCCESS(aws_private_key_fetcher_provider_->Run());

    request_ = make_shared<PrivateKeyFetchingRequest>();
    request_->key_id = make_shared<string>(kKeyId);
    request_->key_vending_endpoint = make_shared<PrivateKeyVendingEndpoint>();
    request_->key_vending_endpoint->private_key_vending_service_endpoint =
        kPrivateKeyBaseUri;
    request_->key_vending_endpoint->service_region = kRegion;
    request_->key_vending_endpoint->account_identity = kAccountIdentity;
  }

  ~AwsPrivateKeyFetcherProviderTest() {
    if (aws_private_key_fetcher_provider_) {
      EXPECT_SUCCESS(aws_private_key_fetcher_provider_->Stop());
    }
    SDKOptions options;
    ShutdownAPI(options);
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
  shared_ptr<MockRoleCredentialsProvider> credentials_provider_;
  unique_ptr<AwsPrivateKeyFetcherProvider> aws_private_key_fetcher_provider_;
  shared_ptr<PrivateKeyFetchingRequest> request_;
};

TEST_F(AwsPrivateKeyFetcherProviderTest, MissingHttpClient) {
  aws_private_key_fetcher_provider_ =
      make_unique<AwsPrivateKeyFetcherProvider>(nullptr, credentials_provider_);

  EXPECT_THAT(aws_private_key_fetcher_provider_->Init(),
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND)));
}

TEST_F(AwsPrivateKeyFetcherProviderTest, MissingCredentialsProvider) {
  aws_private_key_fetcher_provider_ =
      make_unique<AwsPrivateKeyFetcherProvider>(http_client_, nullptr);

  EXPECT_THAT(
      aws_private_key_fetcher_provider_->Init(),
      ResultIs(FailureExecutionResult(
          SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND)));
}

TEST_F(AwsPrivateKeyFetcherProviderTest, SignHttpRequest) {
  atomic<bool> condition = false;

  AsyncContext<PrivateKeyFetchingRequest, HttpRequest> context(
      request_,
      [&](AsyncContext<PrivateKeyFetchingRequest, HttpRequest>& context) {
        EXPECT_SUCCESS(context.result);
        condition = true;
        return SuccessExecutionResult();
      });

  EXPECT_THAT(aws_private_key_fetcher_provider_->SignHttpRequest(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsPrivateKeyFetcherProviderTest, FailedToGetCredentials) {
  credentials_provider_->fail_credentials = true;

  atomic<bool> condition = false;

  AsyncContext<PrivateKeyFetchingRequest, HttpRequest> context(
      request_,
      [&](AsyncContext<PrivateKeyFetchingRequest, HttpRequest>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        condition = true;
      });

  EXPECT_THAT(aws_private_key_fetcher_provider_->SignHttpRequest(context),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
