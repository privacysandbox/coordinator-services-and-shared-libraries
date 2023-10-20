/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "aws_auto_scaling_client_provider.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/autoscaling/AutoScalingClient.h>
#include <aws/autoscaling/model/CompleteLifecycleActionRequest.h>
#include <aws/autoscaling/model/DescribeAutoScalingInstancesRequest.h>

#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_utils.h"
#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/common/src/aws/aws_utils.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

#include "auto_scaling_error_converter.h"
#include "error_codes.h"

using Aws::AutoScaling::AutoScalingClient;
using Aws::AutoScaling::Model::CompleteLifecycleActionOutcome;
using Aws::AutoScaling::Model::CompleteLifecycleActionRequest;
using Aws::AutoScaling::Model::DescribeAutoScalingInstancesOutcome;
using Aws::AutoScaling::Model::DescribeAutoScalingInstancesRequest;
using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationRequest;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED;
using google::scp::core::errors::
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_LIFECYCLE_HOOK_NAME_REQUIRED;
using google::scp::core::errors::
    SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_MULTIPLE_INSTANCES_FOUND;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

static constexpr char kAwsAutoScalingClientProvider[] =
    "AwsAutoScalingClientProvider";
static constexpr char kLifecycleStateTerminatingWait[] = "Terminating:Wait";
static constexpr char kLifecycleStateTerminatingProceed[] =
    "Terminating:Proceed";
static constexpr char kLifecycleActionResultContinue[] = "CONTINUE";

namespace google::scp::cpio::client_providers {
shared_ptr<ClientConfiguration>
AwsAutoScalingClientProvider::CreateClientConfiguration(
    const string& region) noexcept {
  return common::CreateClientConfiguration(make_shared<string>(move(region)));
}

ExecutionResult AwsAutoScalingClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsAutoScalingClientProvider::Run() noexcept {
  auto region_code_or =
      AwsInstanceClientUtils::GetCurrentRegionCode(instance_client_provider_);
  if (!region_code_or.Successful()) {
    SCP_ERROR(kAwsAutoScalingClientProvider, kZeroUuid, region_code_or.result(),
              "Failed to get region code for current instance");
    return region_code_or.result();
  }

  auto_scaling_client_ = auto_scaling_client_factory_->CreateAutoScalingClient(
      *CreateClientConfiguration(*region_code_or), io_async_executor_);

  return SuccessExecutionResult();
}

ExecutionResult AwsAutoScalingClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsAutoScalingClientProvider::TryFinishInstanceTermination(
    AsyncContext<TryFinishInstanceTerminationRequest,
                 TryFinishInstanceTerminationResponse>&
        try_termination_context) noexcept {
  if (try_termination_context.request->instance_resource_id().empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_RESOURCE_ID_REQUIRED);
    SCP_ERROR_CONTEXT(kAwsAutoScalingClientProvider, try_termination_context,
                      execution_result, "Invalid request.");
    try_termination_context.result = execution_result;
    try_termination_context.Finish();
    return execution_result;
  }

  if (try_termination_context.request->lifecycle_hook_name().empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_LIFECYCLE_HOOK_NAME_REQUIRED);
    SCP_ERROR_CONTEXT(kAwsAutoScalingClientProvider, try_termination_context,
                      execution_result, "Invalid request.");
    try_termination_context.result = execution_result;
    try_termination_context.Finish();
    return execution_result;
  }

  DescribeAutoScalingInstancesRequest request;
  request.AddInstanceIds(
      try_termination_context.request->instance_resource_id());

  auto_scaling_client_->DescribeAutoScalingInstancesAsync(
      request,
      bind(
          &AwsAutoScalingClientProvider::OnDescribeAutoScalingInstancesCallback,
          this, try_termination_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsAutoScalingClientProvider::OnDescribeAutoScalingInstancesCallback(
    AsyncContext<TryFinishInstanceTerminationRequest,
                 TryFinishInstanceTerminationResponse>& try_termination_context,
    const AutoScalingClient*, const DescribeAutoScalingInstancesRequest&,
    const DescribeAutoScalingInstancesOutcome& outcome,
    const shared_ptr<const AsyncCallerContext>&) noexcept {
  if (!outcome.IsSuccess()) {
    try_termination_context.result =
        AutoScalingErrorConverter::ConvertAutoScalingError(outcome.GetError());
    SCP_ERROR_CONTEXT(
        kAwsAutoScalingClientProvider, try_termination_context,
        try_termination_context.result,
        "Failed to describe auto-scaling instance for %s.",
        try_termination_context.request->instance_resource_id().c_str());
    try_termination_context.Finish();
    return;
  }

  if (outcome.GetResult().GetAutoScalingInstances().empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_INSTANCE_NOT_FOUND);
    SCP_ERROR_CONTEXT(
        kAwsAutoScalingClientProvider, try_termination_context,
        execution_result, "Failed to describe auto-scaling instance for %s.",
        try_termination_context.request->instance_resource_id().c_str());
    try_termination_context.result = execution_result;
    try_termination_context.Finish();
    return;
  }

  if (outcome.GetResult().GetAutoScalingInstances().size() > 1) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_AUTO_SCALING_CLIENT_PROVIDER_MULTIPLE_INSTANCES_FOUND);
    SCP_ERROR_CONTEXT(
        kAwsAutoScalingClientProvider, try_termination_context,
        execution_result, "Failed to describe auto-scaling instance for %s.",
        try_termination_context.request->instance_resource_id().c_str());
    try_termination_context.result = execution_result;
    try_termination_context.Finish();
    return;
  }

  const auto& instance = outcome.GetResult().GetAutoScalingInstances()[0];
  const auto& lifecycle_state = instance.GetLifecycleState();
  try_termination_context.response =
      make_shared<TryFinishInstanceTerminationResponse>();
  // Return directly if termination is already scheduled.
  if (lifecycle_state == kLifecycleStateTerminatingProceed) {
    try_termination_context.response->set_termination_scheduled(true);
    try_termination_context.result = SuccessExecutionResult();
    try_termination_context.Finish();
    return;
  }
  // Does nothing when the instance is not in terminating wait state.
  if (lifecycle_state != kLifecycleStateTerminatingWait) {
    try_termination_context.response->set_termination_scheduled(false);
    try_termination_context.result = SuccessExecutionResult();
    try_termination_context.Finish();
    return;
  }

  CompleteLifecycleActionRequest complete_lifecycle_action_request;
  complete_lifecycle_action_request.SetAutoScalingGroupName(
      instance.GetAutoScalingGroupName());
  complete_lifecycle_action_request.SetLifecycleActionResult(
      kLifecycleActionResultContinue);
  complete_lifecycle_action_request.SetInstanceId(
      try_termination_context.request->instance_resource_id());
  complete_lifecycle_action_request.SetLifecycleHookName(
      try_termination_context.request->lifecycle_hook_name());

  auto_scaling_client_->CompleteLifecycleActionAsync(
      complete_lifecycle_action_request,
      bind(&AwsAutoScalingClientProvider::OnCompleteLifecycleActionCallback,
           this, try_termination_context, _1, _2, _3, _4),
      nullptr);
}

void AwsAutoScalingClientProvider::OnCompleteLifecycleActionCallback(
    AsyncContext<TryFinishInstanceTerminationRequest,
                 TryFinishInstanceTerminationResponse>& try_termination_context,
    const AutoScalingClient*, const CompleteLifecycleActionRequest&,
    const CompleteLifecycleActionOutcome& outcome,
    const shared_ptr<const AsyncCallerContext>&) noexcept {
  if (!outcome.IsSuccess()) {
    try_termination_context.result =
        AutoScalingErrorConverter::ConvertAutoScalingError(outcome.GetError());
    SCP_ERROR_CONTEXT(
        kAwsAutoScalingClientProvider, try_termination_context,
        try_termination_context.result,
        "Failed to complete lifecycle action for %s.",
        try_termination_context.request->instance_resource_id().c_str());
    try_termination_context.Finish();
    return;
  }

  try_termination_context.response->set_termination_scheduled(true);
  try_termination_context.result = SuccessExecutionResult();
  try_termination_context.Finish();
}

shared_ptr<AutoScalingClient> AutoScalingClientFactory::CreateAutoScalingClient(
    ClientConfiguration& client_config,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  client_config.executor = make_shared<AwsAsyncExecutor>(io_async_executor);
  return make_shared<AutoScalingClient>(client_config);
}

#ifndef TEST_CPIO
shared_ptr<AutoScalingClientProviderInterface>
AutoScalingClientProviderFactory::Create(
    const shared_ptr<AutoScalingClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return make_shared<AwsAutoScalingClientProvider>(
      options, instance_client_provider, io_async_executor);
}
#endif
}  // namespace google::scp::cpio::client_providers
