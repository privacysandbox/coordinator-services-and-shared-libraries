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

#include "cpio/client_providers/auto_scaling_client_provider/src/aws/aws_auto_scaling_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <aws/autoscaling/AutoScalingClient.h>
#include <aws/core/Aws.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/auto_scaling_client_provider/mock/aws/mock_auto_scaling_client.h"
#include "cpio/client_providers/auto_scaling_client_provider/src/aws/error_codes.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_provider.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

using Aws::InitAPI;
using Aws::NoResult;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Vector;
using Aws::AutoScaling::AutoScalingClient;
using Aws::AutoScaling::AutoScalingErrors;
using Aws::AutoScaling::Model::AutoScalingInstanceDetails;
using Aws::AutoScaling::Model::CompleteLifecycleActionOutcome;
using Aws::AutoScaling::Model::CompleteLifecycleActionRequest;
using Aws::AutoScaling::Model::CompleteLifecycleActionResult;
using Aws::AutoScaling::Model::DescribeAutoScalingInstancesOutcome;
using Aws::AutoScaling::Model::DescribeAutoScalingInstancesRequest;
using Aws::AutoScaling::Model::DescribeAutoScalingInstancesResult;
using Aws::Client::AWSError;
using Aws::Client::ClientConfiguration;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationRequest;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED;
using google::scp::core::errors::
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_LIFECYCLE_HOOK_NAME_REQUIRED;
using google::scp::core::errors::
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_MULTIPLE_INSTANCES_FOUND;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::AutoScalingClientFactory;
using google::scp::cpio::client_providers::mock::MockAutoScalingClient;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using std::atomic_bool;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::_;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {
constexpr char kResourceNameMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
constexpr char kInstanceResourceId[] = "instance_id";
constexpr char kLifecycleHookName[] = "lifecycle_hook";
constexpr char kAutoScalingGroupName[] = "group_name";
constexpr char kLifecycleActionResult[] = "CONTINUE";
constexpr char kLifecycleStateTerminatingWait[] = "Terminating:Wait";
constexpr char kLifecycleStateTerminatingProceed[] = "Terminating:Proceed";
constexpr char kLifecycleStatePending[] = "Pending";
}  // namespace

namespace google::scp::cpio::client_providers::test {

class MockAutoScalingClientFactory : public AutoScalingClientFactory {
 public:
  MOCK_METHOD(
      std::shared_ptr<Aws::AutoScaling::AutoScalingClient>,
      CreateAutoScalingClient,
      (Aws::Client::ClientConfiguration & client_config,
       const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor),
      (noexcept, override));
};

class AwsAutoScalingClientProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  AwsAutoScalingClientProviderTest() {
    mock_instance_client_ = make_shared<MockInstanceClientProvider>();
    mock_instance_client_->instance_resource_name = kResourceNameMock;
    mock_auto_scaling_client_ = make_shared<NiceMock<MockAutoScalingClient>>();
    mock_auto_scaling_client_factory_ =
        make_shared<NiceMock<MockAutoScalingClientFactory>>();
    ON_CALL(*mock_auto_scaling_client_factory_, CreateAutoScalingClient)
        .WillByDefault(Return(mock_auto_scaling_client_));

    auto_scaling_client_provider_ = make_unique<AwsAutoScalingClientProvider>(
        make_shared<AutoScalingClientOptions>(), mock_instance_client_,
        mock_io_async_executor_, mock_auto_scaling_client_factory_);
    try_termination_context_.request =
        make_shared<TryFinishInstanceTerminationRequest>();
    try_termination_context_.request->set_instance_resource_id(
        kInstanceResourceId);
    try_termination_context_.request->set_lifecycle_hook_name(
        kLifecycleHookName);
  }

  void TearDown() override {
    EXPECT_SUCCESS(auto_scaling_client_provider_->Stop());
  }

  shared_ptr<MockInstanceClientProvider> mock_instance_client_;
  shared_ptr<MockAutoScalingClient> mock_auto_scaling_client_;
  shared_ptr<MockAutoScalingClientFactory> mock_auto_scaling_client_factory_;
  shared_ptr<MockAsyncExecutor> mock_io_async_executor_ =
      make_shared<MockAsyncExecutor>();
  unique_ptr<AwsAutoScalingClientProvider> auto_scaling_client_provider_;

  AsyncContext<TryFinishInstanceTerminationRequest,
               TryFinishInstanceTerminationResponse>
      try_termination_context_;

  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  atomic_bool finish_called_{false};
};

TEST_F(AwsAutoScalingClientProviderTest,
       RunWithCreateClientConfigurationFailed) {
  auto failure_result = FailureExecutionResult(SC_UNKNOWN);
  mock_instance_client_->get_instance_resource_name_mock = failure_result;
  auto client = make_unique<AwsAutoScalingClientProvider>(
      make_shared<AutoScalingClientOptions>(), mock_instance_client_,
      mock_io_async_executor_, mock_auto_scaling_client_factory_);

  EXPECT_SUCCESS(client->Init());
  EXPECT_THAT(client->Run(), ResultIs(failure_result));
}

TEST_F(AwsAutoScalingClientProviderTest, MissingInstanceResourceId) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.request->clear_instance_resource_id();
  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        auto failure = FailureExecutionResult(
            SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED);
        EXPECT_THAT(context.result, ResultIs(failure));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_auto_scaling_client_, DescribeAutoScalingInstancesAsync)
      .Times(0);

  EXPECT_THAT(
      auto_scaling_client_provider_->TryFinishInstanceTermination(
          try_termination_context_),
      ResultIs(FailureExecutionResult(
          SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED)));
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsAutoScalingClientProviderTest, MissingLifecycleHookName) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.request->clear_lifecycle_hook_name();
  try_termination_context_
      .callback = [this](AsyncContext<TryFinishInstanceTerminationRequest,
                                      TryFinishInstanceTerminationResponse>&
                             context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(
            SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_LIFECYCLE_HOOK_NAME_REQUIRED)));
    finish_called_ = true;
  };

  EXPECT_CALL(*mock_auto_scaling_client_,
              DescribeAutoScalingInstancesAsync(_, _, _))
      .Times(0);

  EXPECT_THAT(
      auto_scaling_client_provider_->TryFinishInstanceTermination(
          try_termination_context_),
      ResultIs(FailureExecutionResult(
          SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_LIFECYCLE_HOOK_NAME_REQUIRED)));
  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P(InstanceIdMatches, instance_id, "") {
  return ExplainMatchResult(UnorderedElementsAre(instance_id),
                            arg.GetInstanceIds(), result_listener);
}

TEST_F(AwsAutoScalingClientProviderTest, DescribeAutoScalingInstancesFailed) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_auto_scaling_client_,
              DescribeAutoScalingInstancesAsync(
                  InstanceIdMatches(kInstanceResourceId), _, _))
      .WillOnce([](auto, auto callback, auto) {
        DescribeAutoScalingInstancesRequest request;
        AWSError<AutoScalingErrors> error(AutoScalingErrors::INTERNAL_FAILURE,
                                          false);
        DescribeAutoScalingInstancesOutcome outcome(move(error));
        callback(nullptr, request, move(outcome), nullptr);
      });

  EXPECT_THAT(auto_scaling_client_provider_->TryFinishInstanceTermination(
                  try_termination_context_),
              IsSuccessful());
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsAutoScalingClientProviderTest, InstanceAlreadyTerminated) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_TRUE(context.response->termination_scheduled());
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_auto_scaling_client_,
              DescribeAutoScalingInstancesAsync(
                  InstanceIdMatches(kInstanceResourceId), _, _))
      .WillOnce([](auto, auto callback, auto) {
        DescribeAutoScalingInstancesRequest request;
        DescribeAutoScalingInstancesResult result;
        AutoScalingInstanceDetails instance_details;
        instance_details.SetLifecycleState(kLifecycleStateTerminatingProceed);
        result.AddAutoScalingInstances(instance_details);
        DescribeAutoScalingInstancesOutcome outcome(move(result));
        callback(nullptr, request, move(outcome), nullptr);
      });

  EXPECT_THAT(auto_scaling_client_provider_->TryFinishInstanceTermination(
                  try_termination_context_),
              IsSuccessful());
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsAutoScalingClientProviderTest, NotInTerminatingWaitState) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_FALSE(context.response->termination_scheduled());
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_auto_scaling_client_,
              DescribeAutoScalingInstancesAsync(
                  InstanceIdMatches(kInstanceResourceId), _, _))
      .WillOnce([](auto, auto callback, auto) {
        DescribeAutoScalingInstancesRequest request;
        DescribeAutoScalingInstancesResult result;
        AutoScalingInstanceDetails instance_details;
        instance_details.SetLifecycleState(kLifecycleStatePending);
        result.AddAutoScalingInstances(instance_details);
        DescribeAutoScalingInstancesOutcome outcome(move(result));
        callback(nullptr, request, move(outcome), nullptr);
      });

  EXPECT_THAT(auto_scaling_client_provider_->TryFinishInstanceTermination(
                  try_termination_context_),
              IsSuccessful());
  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P4(CompleteLifecycleActionRequestMatches, instance_id, group_name,
           lifecycle_action_result, lifecycle_hook, "") {
  return ExplainMatchResult(Eq(instance_id), arg.GetInstanceId(),
                            result_listener) &&
         ExplainMatchResult(Eq(group_name), arg.GetAutoScalingGroupName(),
                            result_listener) &&
         ExplainMatchResult(Eq(lifecycle_action_result),
                            arg.GetLifecycleActionResult(), result_listener) &&
         ExplainMatchResult(Eq(lifecycle_hook), arg.GetLifecycleHookName(),
                            result_listener);
}

TEST_F(AwsAutoScalingClientProviderTest, CompleteLifecycleActionFailed) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_auto_scaling_client_,
              DescribeAutoScalingInstancesAsync(
                  InstanceIdMatches(kInstanceResourceId), _, _))
      .WillOnce([](auto, auto callback, auto) {
        DescribeAutoScalingInstancesRequest request;
        DescribeAutoScalingInstancesResult result;
        AutoScalingInstanceDetails instance_details;
        instance_details.SetLifecycleState(kLifecycleStateTerminatingWait);
        instance_details.SetAutoScalingGroupName(kAutoScalingGroupName);
        result.AddAutoScalingInstances(instance_details);
        DescribeAutoScalingInstancesOutcome outcome(move(result));
        callback(nullptr, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*mock_auto_scaling_client_,
              CompleteLifecycleActionAsync(
                  CompleteLifecycleActionRequestMatches(
                      kInstanceResourceId, kAutoScalingGroupName,
                      kLifecycleActionResult, kLifecycleHookName),
                  _, _))
      .WillOnce([](auto, auto callback, auto) {
        CompleteLifecycleActionRequest request;
        AWSError<AutoScalingErrors> error(AutoScalingErrors::INTERNAL_FAILURE,
                                          false);
        CompleteLifecycleActionOutcome outcome(move(error));
        callback(nullptr, request, move(outcome), nullptr);
      });

  EXPECT_THAT(auto_scaling_client_provider_->TryFinishInstanceTermination(
                  try_termination_context_),
              IsSuccessful());
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsAutoScalingClientProviderTest, ScheduleTerminationSuccessfully) {
  EXPECT_SUCCESS(auto_scaling_client_provider_->Init());
  EXPECT_SUCCESS(auto_scaling_client_provider_->Run());

  try_termination_context_.callback =
      [this](AsyncContext<TryFinishInstanceTerminationRequest,
                          TryFinishInstanceTerminationResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_TRUE(context.response->termination_scheduled());
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_auto_scaling_client_,
              DescribeAutoScalingInstancesAsync(
                  InstanceIdMatches(kInstanceResourceId), _, _))
      .WillOnce([](auto, auto callback, auto) {
        DescribeAutoScalingInstancesRequest request;
        DescribeAutoScalingInstancesResult result;
        AutoScalingInstanceDetails instance_details;
        instance_details.SetLifecycleState(kLifecycleStateTerminatingWait);
        instance_details.SetAutoScalingGroupName(kAutoScalingGroupName);
        result.AddAutoScalingInstances(instance_details);
        DescribeAutoScalingInstancesOutcome outcome(move(result));
        callback(nullptr, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*mock_auto_scaling_client_,
              CompleteLifecycleActionAsync(
                  CompleteLifecycleActionRequestMatches(
                      kInstanceResourceId, kAutoScalingGroupName,
                      kLifecycleActionResult, kLifecycleHookName),
                  _, _))
      .WillOnce([](auto, auto callback, auto) {
        CompleteLifecycleActionRequest request;
        AWSError<AutoScalingErrors> error(AutoScalingErrors::INTERNAL_FAILURE,
                                          false);
        CompleteLifecycleActionResult result;
        CompleteLifecycleActionOutcome outcome(move(result));
        callback(nullptr, request, move(outcome), nullptr);
      });

  EXPECT_THAT(auto_scaling_client_provider_->TryFinishInstanceTermination(
                  try_termination_context_),
              IsSuccessful());
  WaitUntil([this]() { return finish_called_.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
