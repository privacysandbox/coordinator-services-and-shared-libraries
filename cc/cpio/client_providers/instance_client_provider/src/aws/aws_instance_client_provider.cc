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

#include "aws_instance_client_provider.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/core/internal/AWSHttpResourceClient.h>
#include <aws/core/utils/Outcome.h>
#include <aws/ec2/EC2Client.h>
#include <aws/ec2/model/DescribeTagsRequest.h>
#include <aws/ec2/model/Filter.h>

#include "cc/core/common/uuid/src/uuid.h"
#include "core/common/global_logger/src/global_logger.h"
#include "cpio/common/src/aws/aws_utils.h"
#include "public/core/interface/execution_result.h"

#include "ec2_error_converter.h"
#include "error_codes.h"

using Aws::EC2::EC2Client;
using Aws::EC2::EC2Errors;
using Aws::EC2::Model::DescribeTagsOutcome;
using Aws::EC2::Model::DescribeTagsRequest;
using Aws::EC2::Model::Filter;
using Aws::Internal::EC2MetadataClient;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_INSTANCE_ID;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_RESOURCE_NAME;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_TAG_NAME;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_PROVIDER_MULTIPLE_TAG_VALUES_FOUND;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_PROVIDER_NOT_ALL_TAG_VALUES_FOUND;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_PROVIDER_RESOURCE_NOT_FOUND;
using google::scp::cpio::common::CreateClientConfiguration;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;

/// Filename for logging errors
static constexpr char kAwsInstanceClientProvider[] =
    "AwsInstanceClientProvider";
/// Resource ID tag name.
static constexpr char kResourceIdFilterName[] = "resource-id";
/// Key tag name.
static constexpr char kKeyFilterName[] = "key";
/// Resource path to fetch instance ID.
static constexpr char kResourcePathForInstanceId[] =
    "/latest/meta-data/instance-id";
/// Resource path to fetch region.
static constexpr char kResourcePathForRegion[] =
    "/latest/meta-data/placement/region";
/// Resource path to fetch instance public ipv4 address.
static constexpr char kResourcePathForInstancePublicIpv4Address[] =
    "/latest/meta-data/public-ipv4";
/// Resource path to fetch instance private ipv4 address.
static constexpr char kResourcePathForInstancePrivateIpv4Address[] =
    "/latest/meta-data/local-ipv4";

namespace google::scp::cpio::client_providers {
AwsInstanceClientProvider::AwsInstanceClientProvider() {
  ec2_metadata_client_ =
      make_shared<EC2MetadataClient>(*CreateClientConfiguration());
}

ExecutionResult AwsInstanceClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsInstanceClientProvider::Run() noexcept {
  auto region = make_shared<string>();
  auto execution_result = GetCurrentInstanceRegion(*region);
  if (!execution_result.Successful()) {
    ERROR(kAwsInstanceClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get region.");
    return execution_result;
  }
  ec2_client_ = make_shared<EC2Client>(*CreateClientConfiguration(region));
  return SuccessExecutionResult();
}

ExecutionResult AwsInstanceClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsInstanceClientProvider::GetCurrentInstanceProjectId(
    std::string&) noexcept {
  // Not implemented.
  return FailureExecutionResult(SC_UNKNOWN);
}

ExecutionResult AwsInstanceClientProvider::GetCurrentInstanceZone(
    std::string&) noexcept {
  // Not implemented.
  return FailureExecutionResult(SC_UNKNOWN);
}

ExecutionResult AwsInstanceClientProvider::GetCurrentInstanceId(
    string& instance_id) noexcept {
  return GetResource(instance_id, kResourcePathForInstanceId);
}

ExecutionResult AwsInstanceClientProvider::GetCurrentInstanceRegion(
    string& region) noexcept {
  return GetResource(region, kResourcePathForRegion);
}

ExecutionResult AwsInstanceClientProvider::GetCurrentInstancePublicIpv4Address(
    string& instance_public_ipv4_address) noexcept {
  return GetResource(instance_public_ipv4_address,
                     kResourcePathForInstancePublicIpv4Address);
}

ExecutionResult AwsInstanceClientProvider::GetCurrentInstancePrivateIpv4Address(
    string& instance_private_ipv4_address) noexcept {
  return GetResource(instance_private_ipv4_address,
                     kResourcePathForInstancePrivateIpv4Address);
}

ExecutionResult AwsInstanceClientProvider::GetTagsOfInstance(
    const vector<string>& tag_names, const string& instance_id,
    map<string, string>& tag_values_map) noexcept {
  if (tag_names.empty()) {
    return SuccessExecutionResult();
  }

  for (auto tag_name : tag_names) {
    if (tag_name.empty()) {
      auto execution_result = FailureExecutionResult(
          SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_TAG_NAME);
      ERROR(kAwsInstanceClientProvider, kZeroUuid, kZeroUuid, execution_result,
            "Failed to get tag.");
      return execution_result;
    }
  }

  if (instance_id.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_INSTANCE_ID);
    ERROR(kAwsInstanceClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get tag.");
    return execution_result;
  }

  DescribeTagsRequest request;

  Filter resource_id_filter;
  resource_id_filter.SetName(kResourceIdFilterName);
  resource_id_filter.AddValues(instance_id.c_str());
  request.AddFilters(resource_id_filter);

  Filter key_filter;
  key_filter.SetName(kKeyFilterName);
  for (auto tag_name : tag_names) {
    key_filter.AddValues(tag_name.c_str());
  }
  request.AddFilters(key_filter);

  auto outcome = ec2_client_->DescribeTags(request);

  if (!outcome.IsSuccess()) {
    auto error_type = outcome.GetError().GetErrorType();
    return EC2ErrorConverter::ConvertEC2Error(error_type,
                                              outcome.GetError().GetMessage());
  }

  if (outcome.GetResult().GetTags().size() < tag_names.size()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_INSTANCE_CLIENT_PROVIDER_NOT_ALL_TAG_VALUES_FOUND);
    ERROR(kAwsInstanceClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get tag.");
    return execution_result;
  }

  if (outcome.GetResult().GetTags().size() > tag_names.size()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_INSTANCE_CLIENT_PROVIDER_MULTIPLE_TAG_VALUES_FOUND);
    ERROR(kAwsInstanceClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get tag.");
    return execution_result;
  }

  for (auto tag : outcome.GetResult().GetTags()) {
    tag_values_map.emplace(tag.GetKey(), string(tag.GetValue().c_str()));
  }
  return SuccessExecutionResult();
}

ExecutionResult AwsInstanceClientProvider::GetResource(
    string& resource_value, const string& resource_name) noexcept {
  if (resource_name.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_INSTANCE_CLIENT_PROVIDER_INVALID_RESOURCE_NAME);
    ERROR(kAwsInstanceClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get resource.");
    return execution_result;
  }
  resource_value =
      string(ec2_metadata_client_->GetResource(resource_name.c_str()));
  if (resource_value.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_INSTANCE_CLIENT_PROVIDER_RESOURCE_NOT_FOUND);
    ERROR(kAwsInstanceClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get resource.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

shared_ptr<InstanceClientProviderInterface>
InstanceClientProviderFactory::Create(
    const shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
    const shared_ptr<core::HttpClientInterface>& http1_client,
    const shared_ptr<core::HttpClientInterface>& http2_client,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  return make_shared<AwsInstanceClientProvider>();
}

}  // namespace google::scp::cpio::client_providers
