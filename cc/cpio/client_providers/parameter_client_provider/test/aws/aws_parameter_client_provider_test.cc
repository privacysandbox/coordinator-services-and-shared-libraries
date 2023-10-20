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

#include "cpio/client_providers/parameter_client_provider/src/aws/aws_parameter_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/ssm/SSMClient.h>
#include <aws/ssm/SSMErrors.h>
#include <aws/ssm/model/GetParametersRequest.h>

#include "core/async_executor/mock/mock_async_executor.h"
// #include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/parameter_client_provider/mock/aws/mock_ssm_client.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Client::AWSError;
using Aws::SSM::SSMErrors;
using Aws::SSM::Model::GetParametersOutcome;
using Aws::SSM::Model::GetParametersRequest;
using Aws::SSM::Model::GetParametersResult;
using Aws::SSM::Model::Parameter;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_MULTIPLE_PARAMETERS_FOUND;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockSSMClient;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::NiceMock;
using testing::Return;

namespace {
constexpr char kResourceNameMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
constexpr char kParameterName[] = "name";
constexpr char kParameterValue[] = "value";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class MockSSMClientFactory : public SSMClientFactory {
 public:
  MOCK_METHOD(
      std::shared_ptr<Aws::SSM::SSMClient>, CreateSSMClient,
      (Aws::Client::ClientConfiguration & client_config,
       const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor),
      (noexcept, override));
};

class AwsParameterClientProviderTest : public ::testing::Test {
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
    mock_instance_client_ = make_shared<MockInstanceClientProvider>();
    mock_instance_client_->instance_resource_name = kResourceNameMock;

    mock_ssm_client_ = std::make_shared<MockSSMClient>();
    mock_ssm_client_factory_ = make_shared<NiceMock<MockSSMClientFactory>>();
    ON_CALL(*mock_ssm_client_factory_, CreateSSMClient)
        .WillByDefault(Return(mock_ssm_client_));

    MockAsyncExecutor mock_io_async_executor;
    shared_ptr<AsyncExecutorInterface> io_async_executor =
        make_shared<MockAsyncExecutor>(move(mock_io_async_executor));

    client_ = make_unique<AwsParameterClientProvider>(
        make_shared<ParameterClientOptions>(), mock_instance_client_,
        io_async_executor, mock_ssm_client_factory_);
  }

  void MockParameters() {
    // Mocks GetParametersRequest.
    GetParametersRequest get_parameters_request;
    get_parameters_request.AddNames(kParameterName);
    mock_ssm_client_->get_parameters_request_mock = get_parameters_request;

    // Mocks successs GetParametersOutcome
    GetParametersResult result;
    Parameter parameter;
    parameter.SetName(kParameterName);
    parameter.SetValue(kParameterValue);
    result.AddParameters(parameter);
    GetParametersOutcome get_parameters_outcome(result);
    mock_ssm_client_->get_parameters_outcome_mock = get_parameters_outcome;
  }

  void TearDown() override { EXPECT_SUCCESS(client_->Stop()); }

  shared_ptr<MockInstanceClientProvider> mock_instance_client_;
  shared_ptr<MockSSMClient> mock_ssm_client_;
  shared_ptr<MockSSMClientFactory> mock_ssm_client_factory_;
  shared_ptr<MockAsyncExecutor> mock_io_async_executor_;
  unique_ptr<AwsParameterClientProvider> client_;
};

TEST_F(AwsParameterClientProviderTest, FailedToFetchRegion) {
  auto failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  mock_instance_client_->get_instance_resource_name_mock = failure;

  EXPECT_SUCCESS(client_->Init());
  EXPECT_THAT(client_->Run(), ResultIs(failure));
}

TEST_F(AwsParameterClientProviderTest, FailedToFetchParameters) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());

  MockParameters();
  AWSError<SSMErrors> error(SSMErrors::INTERNAL_FAILURE, false);
  GetParametersOutcome outcome(error);
  mock_ssm_client_->get_parameters_outcome_mock = outcome;

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterName);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
        condition = true;
      });
  EXPECT_SUCCESS(client_->GetParameter(context));

  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsParameterClientProviderTest, InvalidParameterName) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(
                SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME)));
        condition = true;
      });
  EXPECT_THAT(client_->GetParameter(context),
              ResultIs(FailureExecutionResult(
                  SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME)));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsParameterClientProviderTest, ParameterNotFound) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());
  MockParameters();

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name("invalid_parameter");
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND)));
        condition = true;
      });
  EXPECT_SUCCESS(client_->GetParameter(context));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsParameterClientProviderTest, MultipleParametersFound) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());

  MockParameters();
  GetParametersResult result;
  Parameter parameter1;
  parameter1.SetName(kParameterName);
  parameter1.SetValue(kParameterValue);
  // Two parameters found.
  result.AddParameters(parameter1);
  result.AddParameters(parameter1);
  GetParametersOutcome get_parameters_outcome(result);
  mock_ssm_client_->get_parameters_outcome_mock = get_parameters_outcome;

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterName);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(
                SC_AWS_PARAMETER_CLIENT_PROVIDER_MULTIPLE_PARAMETERS_FOUND)));
        condition = true;
      });
  EXPECT_SUCCESS(client_->GetParameter(context));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsParameterClientProviderTest, SucceedToFetchParameter) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());

  MockParameters();

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterName);
  AsyncContext<GetParameterRequest, GetParameterResponse> context1(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->parameter_value(), kParameterValue);
        condition = true;
      });
  EXPECT_SUCCESS(client_->GetParameter(context1));
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
