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

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <aws/core/Aws.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/STSErrors.h>
#include <aws/sts/model/AssumeRoleRequest.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/credentials_provider/mock/mock_aws_assume_role_credentials_provider.h"
#include "core/credentials_provider/mock/mock_aws_sts_client.h"
#include "core/credentials_provider/src/error_codes.h"
#include "core/interface/async_executor_interface.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Client::AsyncCallerContext;
using Aws::Client::AWSError;
using Aws::STS::AssumeRoleResponseReceivedHandler;
using Aws::STS::STSClient;
using Aws::STS::STSErrors;
using Aws::STS::Model::AssumeRoleOutcome;
using Aws::STS::Model::AssumeRoleRequest;
using Aws::STS::Model::AssumeRoleResult;
using Aws::STS::Model::Credentials;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::credentials_provider::mock::
    MockAwsAssumeRoleCredentialsProvider;
using google::scp::core::credentials_provider::mock::MockSTSClient;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::core::test {
TEST(AwsAssumeRoleCredentialsProviderTest, AssumRole) {
  SDKOptions options;
  InitAPI(options);

  auto mock_sts_client = make_shared<MockSTSClient>();
  auto sts_client = dynamic_pointer_cast<STSClient>(mock_sts_client);
  auto assume_role_arn = make_shared<string>("assume_role_arn");
  auto assume_role_external_id = make_shared<string>("assume_role_external_id");
  auto region = make_shared<string>("region");
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<AsyncExecutorInterface> io_async_executor;

  auto mock_assume_role_credentials_provider =
      make_shared<MockAwsAssumeRoleCredentialsProvider>(
          sts_client, assume_role_arn, assume_role_external_id, async_executor,
          io_async_executor, region);

  auto is_called = false;
  mock_sts_client->mock_assume_role_async =
      [&](const AssumeRoleRequest& request,
          const AssumeRoleResponseReceivedHandler&,
          const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(request.GetRoleArn(), "assume_role_arn");
        EXPECT_EQ(request.GetRoleSessionName(), "session_name");
        EXPECT_EQ(request.GetExternalId(), "assume_role_external_id");
        is_called = true;
      };

  AsyncContext<GetCredentialsRequest, GetCredentialsResponse>
      get_credentials_context(
          make_shared<GetCredentialsRequest>(),
          [&](AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
                  context) {});

  mock_assume_role_credentials_provider->GetCredentials(
      get_credentials_context);
  EXPECT_EQ(is_called, true);
  ShutdownAPI(options);
}

TEST(AwsAssumeRoleCredentialsProviderTest, OnGetCredentialsCallback) {
  SDKOptions options;
  InitAPI(options);

  auto mock_sts_client = make_shared<MockSTSClient>();
  auto sts_client = dynamic_pointer_cast<STSClient>(mock_sts_client);
  auto assume_role_arn = make_shared<string>("assume_role_arn");
  auto assume_role_external_id = make_shared<string>("assume_role_external_id");
  auto region = make_shared<string>("region");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> io_async_executor;

  auto mock_assume_role_credentials_provider =
      make_shared<MockAwsAssumeRoleCredentialsProvider>(
          sts_client, assume_role_arn, assume_role_external_id, async_executor,
          io_async_executor, region);

  auto is_called = false;
  AsyncContext<GetCredentialsRequest, GetCredentialsResponse>
      get_credentials_context(
          make_shared<GetCredentialsRequest>(),
          [&](AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
                  context) {
            EXPECT_THAT(
                context.result,
                ResultIs(FailureExecutionResult(
                    errors::
                        SC_CREDENTIALS_PROVIDER_FAILED_TO_FETCH_CREDENTIALS)));
            is_called = true;
          });

  AssumeRoleRequest get_credentials_request;

  AWSError<STSErrors> sts_error(STSErrors::INVALID_ACTION, false);
  AssumeRoleOutcome get_credentials_outcome(sts_error);

  mock_assume_role_credentials_provider->OnGetCredentialsCallback(
      get_credentials_context, sts_client.get(), get_credentials_request,
      get_credentials_outcome, nullptr);

  EXPECT_EQ(is_called, true);
  ShutdownAPI(options);
}

TEST(AwsAssumeRoleCredentialsProviderTest, OnGetCredentialsCallbackSuccess) {
  SDKOptions options;
  InitAPI(options);

  auto mock_sts_client = make_shared<MockSTSClient>();
  auto sts_client = dynamic_pointer_cast<STSClient>(mock_sts_client);
  auto assume_role_arn = make_shared<string>("assume_role_arn");
  auto assume_role_external_id = make_shared<string>("assume_role_external_id");
  auto region = make_shared<string>("region");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<AsyncExecutorInterface> io_async_executor;

  auto mock_assume_role_credentials_provider =
      make_shared<MockAwsAssumeRoleCredentialsProvider>(
          sts_client, assume_role_arn, assume_role_external_id, async_executor,
          io_async_executor, region);

  auto is_called = false;
  AsyncContext<GetCredentialsRequest, GetCredentialsResponse>
      get_credentials_context(
          make_shared<GetCredentialsRequest>(),
          [&](AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
                  context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_EQ(*context.response->access_key_id, "access_key");
            EXPECT_EQ(*context.response->access_key_secret, "secret_key");
            EXPECT_EQ(*context.response->security_token, "token");
            is_called = true;
          });

  AssumeRoleRequest get_credentials_request;

  AssumeRoleResult get_credentials_result;
  Credentials credentials;
  credentials.SetAccessKeyId("access_key");
  credentials.SetSecretAccessKey("secret_key");
  credentials.SetSessionToken("token");

  get_credentials_result.SetCredentials(credentials);
  AssumeRoleOutcome get_credentials_outcome(get_credentials_result);

  mock_assume_role_credentials_provider->OnGetCredentialsCallback(
      get_credentials_context, sts_client.get(), get_credentials_request,
      get_credentials_outcome, nullptr);

  EXPECT_EQ(is_called, true);
  ShutdownAPI(options);
}

}  // namespace google::scp::core::test
