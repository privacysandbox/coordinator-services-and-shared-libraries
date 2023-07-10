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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/ssm/SSMClient.h>
#include <aws/ssm/SSMErrors.h>
#include <aws/ssm/model/GetParametersRequest.h>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/parameter_client_provider/mock/aws/mock_aws_parameter_client_provider_with_overrides.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
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
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_MULTIPLE_PARAMETERS_FOUND;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::
    MockAwsParameterClientProviderWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

static constexpr char kRegion[] = "us-east-1";
static constexpr char kParameterName[] = "name";
static constexpr char kParameterValue[] = "value";

namespace google::scp::cpio::client_providers::test {
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
    client_ = make_unique<MockAwsParameterClientProviderWithOverrides>();
    client_->GetInstanceClientProvider()->region_mock = kRegion;
  }

  void MockParameters() {
    // Mocks GetParametersRequest.
    GetParametersRequest get_parameters_request;
    get_parameters_request.AddNames(kParameterName);
    client_->GetSSMClient()->get_parameters_request_mock =
        get_parameters_request;

    // Mocks successs GetParametersOutcome
    GetParametersResult result;
    Parameter parameter;
    parameter.SetName(kParameterName);
    parameter.SetValue(kParameterValue);
    result.AddParameters(parameter);
    GetParametersOutcome get_parameters_outcome(result);
    client_->GetSSMClient()->get_parameters_outcome_mock =
        get_parameters_outcome;
  }

  void TearDown() override {
    EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
  }

  unique_ptr<MockAwsParameterClientProviderWithOverrides> client_;
};

TEST_F(AwsParameterClientProviderTest, FailedToFetchRegion) {
  auto failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  client_->GetInstanceClientProvider()->get_region_result_mock = failure;

  EXPECT_EQ(client_->Init(), failure);
}

TEST_F(AwsParameterClientProviderTest, FailedToFetchParameters) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  MockParameters();
  AWSError<SSMErrors> error(SSMErrors::INTERNAL_FAILURE, false);
  GetParametersOutcome outcome(error);
  client_->GetSSMClient()->get_parameters_outcome_mock = outcome;

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterName);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_EQ(context.result,
                  FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR));
        condition = true;
      });
  EXPECT_EQ(client_->GetParameter(context), SuccessExecutionResult());

  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsParameterClientProviderTest, InvalidParameterName) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_EQ(context.result,
                  FailureExecutionResult(
                      SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME));
        condition = true;
      });
  EXPECT_EQ(client_->GetParameter(context),
            FailureExecutionResult(
                SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsParameterClientProviderTest, ParameterNotFound) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());
  MockParameters();

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name("invalid_parameter");
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_EQ(context.result,
                  FailureExecutionResult(
                      SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND));
        condition = true;
      });
  EXPECT_EQ(client_->GetParameter(context), SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsParameterClientProviderTest, MultipleParametersFound) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  MockParameters();
  GetParametersResult result;
  Parameter parameter1;
  parameter1.SetName(kParameterName);
  parameter1.SetValue(kParameterValue);
  // Two parameters found.
  result.AddParameters(parameter1);
  result.AddParameters(parameter1);
  GetParametersOutcome get_parameters_outcome(result);
  client_->GetSSMClient()->get_parameters_outcome_mock = get_parameters_outcome;

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterName);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_EQ(
            context.result,
            FailureExecutionResult(
                SC_AWS_PARAMETER_CLIENT_PROVIDER_MULTIPLE_PARAMETERS_FOUND));
        condition = true;
      });
  EXPECT_EQ(client_->GetParameter(context), SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsParameterClientProviderTest, SucceedToFetchParameter) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  MockParameters();

  atomic<bool> condition = false;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterName);
  AsyncContext<GetParameterRequest, GetParameterResponse> context1(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_EQ(context.result, SuccessExecutionResult());
        EXPECT_EQ(context.response->parameter_value(), kParameterValue);
        condition = true;
      });
  EXPECT_EQ(client_->GetParameter(context1), SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
