
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

#include "cpio/client_providers/private_key_fetching_client_provider/src/aws/aws_private_key_fetching_client_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "core/http2_client/mock/mock_http_client.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/private_key_fetching_client_provider/src/aws/error_codes.h"
#include "cpio/client_providers/private_key_fetching_client_provider/src/error_codes.h"
#include "cpio/client_providers/role_credentials_provider/mock/mock_role_credentials_provider.h"
#include "public/core/interface/execution_result.h"

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
    SC_AWS_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_INVALID_URI;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::AwsPrivateKeyFetchingClientProvider;
using google::scp::cpio::client_providers::mock::MockRoleCredentialsProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

static constexpr char kAccountIdentity[] = "accountIdentity";
static constexpr char kRegion[] = "us-east-1";
static constexpr char kServiceName[] = "execute-api";

namespace google::scp::cpio::client_providers::test {
class AwsPrivateKeyFetchingClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    http_client_ = make_shared<MockHttpClient>();
    credentials_provider_ = make_shared<MockRoleCredentialsProvider>();
    auto service_name = string(kServiceName);
    aws_private_key_fetching_client_provider =
        make_unique<AwsPrivateKeyFetchingClientProvider>(http_client_,
                                                         credentials_provider_);
    EXPECT_EQ(aws_private_key_fetching_client_provider->Init(),
              SuccessExecutionResult());
  }

  void TearDown() override {
    if (aws_private_key_fetching_client_provider) {
      EXPECT_EQ(aws_private_key_fetching_client_provider->Stop(),
                SuccessExecutionResult());
    }
  }

  void MockRequest(const string& uri) {
    http_client_->request_mock = HttpRequest();
    http_client_->request_mock.path = make_shared<string>(uri);
  }

  void MockResponse(const string& str) {
    BytesBuffer bytes_buffer(sizeof(str));
    bytes_buffer.bytes = make_shared<vector<Byte>>(str.begin(), str.end());
    bytes_buffer.capacity = sizeof(str);

    http_client_->response_mock = HttpResponse();
    http_client_->response_mock.body = move(bytes_buffer);
  }

  shared_ptr<MockHttpClient> http_client_;
  shared_ptr<MockRoleCredentialsProvider> credentials_provider_;
  unique_ptr<AwsPrivateKeyFetchingClientProvider>
      aws_private_key_fetching_client_provider;
};

TEST_F(AwsPrivateKeyFetchingClientProviderTest, Run) {
  EXPECT_EQ(aws_private_key_fetching_client_provider->Run(),
            SuccessExecutionResult());
}

TEST_F(AwsPrivateKeyFetchingClientProviderTest, MissingHttpClient) {
  aws_private_key_fetching_client_provider =
      make_unique<AwsPrivateKeyFetchingClientProvider>(nullptr,
                                                       credentials_provider_);

  EXPECT_EQ(aws_private_key_fetching_client_provider->Init().status_code,
            SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_HTTP_CLIENT_NOT_FOUND);
}

TEST_F(AwsPrivateKeyFetchingClientProviderTest, MissingCredentialsProvider) {
  aws_private_key_fetching_client_provider =
      make_unique<AwsPrivateKeyFetchingClientProvider>(http_client_, nullptr);

  EXPECT_EQ(
      aws_private_key_fetching_client_provider->Init().status_code,
  SC_AWS_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND);
}

TEST_F(AwsPrivateKeyFetchingClientProviderTest, SignHttpRequest) {
  auto request = make_shared<HttpRequest>();
  request->path = make_shared<string>("http://localhost.test:8000");
  atomic<bool> condition = false;

  AsyncContext<HttpRequest, HttpRequest> context(
      request, [&](AsyncContext<HttpRequest, HttpRequest>& context) {
        EXPECT_EQ(context.result, SuccessExecutionResult());
        condition = true;
        return SuccessExecutionResult();
      });

  EXPECT_EQ(aws_private_key_fetching_client_provider->SignHttpRequest(
                context, make_shared<string>(kRegion),
                make_shared<string>(kAccountIdentity)),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsPrivateKeyFetchingClientProviderTest, FailedToGetCredentials) {
  credentials_provider_->fail_credentials = true;

  auto request = make_shared<HttpRequest>();
  atomic<bool> condition = false;

  AsyncContext<HttpRequest, HttpRequest> context(
      request, [&](AsyncContext<HttpRequest, HttpRequest>& context) {
        EXPECT_EQ(context.result.status_code, 123);
        condition = true;
      });

  EXPECT_EQ(aws_private_key_fetching_client_provider
                ->SignHttpRequest(context,

                                  make_shared<string>(kRegion),
                                  make_shared<string>(kAccountIdentity))
                .status_code,
            123);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsPrivateKeyFetchingClientProviderTest, InvalidUriInHttpRequest) {
  auto request = make_shared<HttpRequest>();
  request->path = make_shared<string>("FailedURI");
  atomic<bool> condition = false;

  AsyncContext<HttpRequest, HttpRequest> context(
      request, [&](AsyncContext<HttpRequest, HttpRequest>& context) {
        EXPECT_EQ(context.result.status_code,
                  SC_AWS_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_INVALID_URI);
        condition = true;
      });

  EXPECT_EQ(aws_private_key_fetching_client_provider->SignHttpRequest(
                context, make_shared<string>(kRegion),
                make_shared<string>(kAccountIdentity)),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

}  // namespace google::scp::cpio::client_providers::test
