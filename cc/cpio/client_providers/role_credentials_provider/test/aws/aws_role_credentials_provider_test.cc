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
#include "core/interface/async_executor_interface.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/role_credentials_provider/mock/aws/mock_aws_role_credentials_provider_with_overrides.h"
#include "cpio/client_providers/role_credentials_provider/mock/aws/mock_aws_sts_client.h"
#include "cpio/client_providers/role_credentials_provider/src/aws/error_codes.h"
#include "cpio/common/src/aws/error_codes.h"
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
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::
    SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::
    MockAwsRoleCredentialsProviderWithOverrides;
using google::scp::cpio::client_providers::mock::MockSTSClient;
using std::atomic;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace {
constexpr char kResourceNameMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
constexpr char kAssumeRoleArn[] = "assume_role_arn";
constexpr char kSessionName[] = "session_name";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class AwsRoleCredentialsProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  void SetUp() override {
    role_credentials_provider_ =
        make_shared<MockAwsRoleCredentialsProviderWithOverrides>();
    EXPECT_SUCCESS(role_credentials_provider_->Init());
    role_credentials_provider_->GetInstanceClientProvider()
        ->instance_resource_name = kResourceNameMock;
    EXPECT_SUCCESS(role_credentials_provider_->Run());
    mock_sts_client_ = role_credentials_provider_->GetSTSClient();
  }

  void TearDown() override {
    EXPECT_SUCCESS(role_credentials_provider_->Stop());
  }

  shared_ptr<MockAwsRoleCredentialsProviderWithOverrides>
      role_credentials_provider_;
  shared_ptr<MockSTSClient> mock_sts_client_;
};

TEST_F(AwsRoleCredentialsProviderTest, AssumRoleSuccess) {
  atomic<bool> finished = false;
  mock_sts_client_->mock_assume_role_async =
      [&](const AssumeRoleRequest& request,
          const AssumeRoleResponseReceivedHandler&,
          const shared_ptr<const AsyncCallerContext>&) {
        EXPECT_EQ(request.GetRoleArn(), kAssumeRoleArn);
        EXPECT_EQ(request.GetRoleSessionName(), kSessionName);
        finished = true;
      };

  auto request = make_shared<GetRoleCredentialsRequest>();
  request->account_identity = make_shared<string>(kAssumeRoleArn);
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_credentials_context(
          move(request),
          [&](AsyncContext<GetRoleCredentialsRequest,
                           GetRoleCredentialsResponse>& context) {});
  role_credentials_provider_->GetRoleCredentials(get_credentials_context);

  WaitUntil([&]() { return finished.load(); });
}

TEST_F(AwsRoleCredentialsProviderTest, AssumRoleFailure) {
  auto is_called = false;
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_credentials_context(
          make_shared<GetRoleCredentialsRequest>(),
          [&](AsyncContext<GetRoleCredentialsRequest,
                           GetRoleCredentialsResponse>& context) {
            EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(
                                            SC_AWS_INTERNAL_SERVICE_ERROR)));
            is_called = true;
          });

  AssumeRoleRequest get_credentials_request;
  AWSError<STSErrors> sts_error(STSErrors::INVALID_ACTION, false);
  AssumeRoleOutcome get_credentials_outcome(sts_error);
  role_credentials_provider_->OnGetRoleCredentialsCallback(
      get_credentials_context, mock_sts_client_.get(), get_credentials_request,
      get_credentials_outcome, nullptr);

  EXPECT_EQ(is_called, true);
}

TEST_F(AwsRoleCredentialsProviderTest, NullInstanceClientProvider) {
  auto role_credentials_provider = make_shared<AwsRoleCredentialsProvider>(
      nullptr, make_shared<MockAsyncExecutor>(),
      make_shared<MockAsyncExecutor>());
  EXPECT_SUCCESS(role_credentials_provider->Init());
  EXPECT_THAT(role_credentials_provider->Run(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED)));
}

TEST_F(AwsRoleCredentialsProviderTest, NullCpuAsyncExecutor) {
  auto role_credentials_provider = make_shared<AwsRoleCredentialsProvider>(
      make_shared<mock::MockInstanceClientProvider>(), nullptr,
      make_shared<MockAsyncExecutor>());
  EXPECT_SUCCESS(role_credentials_provider->Init());
  EXPECT_THAT(role_credentials_provider->Run(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED)));
}

TEST_F(AwsRoleCredentialsProviderTest, NullIoAsyncExecutor) {
  auto role_credentials_provider = make_shared<AwsRoleCredentialsProvider>(
      make_shared<mock::MockInstanceClientProvider>(),
      make_shared<MockAsyncExecutor>(), nullptr);
  EXPECT_SUCCESS(role_credentials_provider->Init());
  EXPECT_THAT(role_credentials_provider->Run(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED)));
}

}  // namespace google::scp::cpio::client_providers::test
