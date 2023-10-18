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
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <aws/ec2/model/DescribeInstancesRequest.h>
#include <aws/ec2/model/DescribeTagsRequest.h>
#include <nlohmann/json.hpp>

#include "absl/strings/str_format.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/http_client_interface.h"
#include "cpio/common/src/aws/aws_utils.h"
#include "cpio/common/src/cpio_utils.h"
#include "public/core/interface/execution_result.h"

#include "aws_instance_client_utils.h"
#include "ec2_error_converter.h"
#include "error_codes.h"

using Aws::Client::AsyncCallerContext;
using Aws::EC2::EC2Client;
using Aws::EC2::Model::DescribeInstancesOutcome;
using Aws::EC2::Model::DescribeInstancesRequest;
using Aws::EC2::Model::DescribeTagsOutcome;
using Aws::EC2::Model::DescribeTagsRequest;
using Aws::EC2::Model::Filter;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::cmrt::sdk::instance_service::v1::InstanceNetwork;
using google::protobuf::MapPair;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_INSTANCE_RESOURCE_NAME_RESPONSE_MALFORMED;
using google::scp::core::errors::SC_AWS_INSTANCE_CLIENT_INVALID_REGION_CODE;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_PROVIDER_DESCRIBE_INSTANCES_RESPONSE_MALFORMED;
using google::scp::cpio::common::CpioUtils;
using google::scp::cpio::common::CreateClientConfiguration;
using nlohmann::json;
using std::all_of;
using std::begin;
using std::bind;
using std::cbegin;
using std::cend;
using std::end;
using std::find;
using std::make_pair;
using std::make_shared;
using std::map;
using std::move;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

namespace {
constexpr char kAwsInstanceClientProvider[] = "AwsInstanceClientProvider";
constexpr char kAuthorizationHeaderKey[] = "X-aws-ec2-metadata-token";
constexpr size_t kMaxConcurrentConnections = 1000;
// Resource ID tag name.
constexpr char kResourceIdFilterName[] = "resource-id";
// Use IMDSv2 to fetch current instance ID.
// For more information, see
// https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/configuring-instance-metadata-service.html
constexpr char kAwsInstanceDynamicDataUrl[] =
    "http://169.254.169.254/latest/dynamic/instance-identity/document";
constexpr char kAccountIdKey[] = "accountId";
constexpr char kInstanceIdKey[] = "instanceId";
constexpr char kRegionKey[] = "region";
constexpr char kAwsInstanceResourceNameFormat[] =
    "arn:aws:ec2:%s:%s:instance/%s";

// Available Regions. Refers to
// https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/using-regions-availability-zones.html
const set<string> kAwsRegionCodes = {
    "us-east-2",      "us-east-1",      "us-west-1",      "us-west-2",
    "af-south-1",     "ap-east-1",      "ap-south-2",     "ap-southeast-3",
    "ap-southeast-4", "ap-south-1",     "ap-northeast-3", "ap-northeast-2",
    "ap-southeast-1", "ap-southeast-2", "ap-northeast-1", "ca-central-1",
    "eu-central-1",   "eu-west-1",      "eu-west-2",      "eu-south-1",
    "eu-west-3",      "eu-south-2",     "eu-north-1",     "eu-central-2",
    "me-south-1",     "me-central-1",   "sa-east-1",
};
constexpr char kDefaultRegionCode[] = "us-east-1";

// Returns a pair of iterators - one to the beginning, one to the end.
const auto& GetRequiredFieldsForInstanceDynamicData() {
  static char const* components[3];
  using iterator_type = decltype(cbegin(components));
  static pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kAccountIdKey;
    components[1] = kInstanceIdKey;
    components[2] = kRegionKey;
    return make_pair(cbegin(components), cend(components));
  }();
  return iterator_pair;
}

}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult AwsInstanceClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsInstanceClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsInstanceClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResultOr<shared_ptr<EC2Client>>
AwsInstanceClientProvider::GetEC2ClientByRegion(const string& region) noexcept {
  auto target_region = region.empty() ? kDefaultRegionCode : region;
  auto it = kAwsRegionCodes.find(target_region);
  if (it == kAwsRegionCodes.end()) {
    return FailureExecutionResult(SC_AWS_INSTANCE_CLIENT_INVALID_REGION_CODE);
  }

  shared_ptr<EC2Client> ec2_client;
  if (ec2_clients_list_.Find(target_region, ec2_client).Successful()) {
    return ec2_client;
  }

  auto ec2_client_or =
      ec2_factory_->CreateClient(target_region, io_async_executor_);
  RETURN_IF_FAILURE(ec2_client_or.result());

  ec2_client = move(*ec2_client_or);
  ec2_clients_list_.Insert(make_pair(target_region, ec2_client), ec2_client);

  return ec2_client;
}

ExecutionResult AwsInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    string& resource_name) noexcept {
  GetCurrentInstanceResourceNameRequest request;
  GetCurrentInstanceResourceNameResponse response;
  auto execution_result =
      CpioUtils::AsyncToSync<GetCurrentInstanceResourceNameRequest,
                             GetCurrentInstanceResourceNameResponse>(
          bind(&AwsInstanceClientProvider::GetCurrentInstanceResourceName, this,
               _1),
          request, response);

  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsInstanceClientProvider, kZeroUuid, execution_result,
              "Failed to run async function GetCurrentInstanceResourceName for "
              "current instance resource name");
    return execution_result;
  }

  resource_name = move(*response.mutable_instance_resource_name());

  return SuccessExecutionResult();
}

ExecutionResult AwsInstanceClientProvider::GetCurrentInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context) noexcept {
  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(
          make_shared<GetSessionTokenRequest>(),
          bind(&AwsInstanceClientProvider::OnGetSessionTokenCallback, this,
               get_resource_name_context, _1),
          get_resource_name_context);
  auto execution_result =
      auth_token_provider_->GetSessionToken(get_token_context);
  if (!execution_result.Successful()) {
    get_resource_name_context.result = execution_result;
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_resource_name_context,
                      get_resource_name_context.result,
                      "Failed to get the session token for current instance.");
    get_resource_name_context.Finish();

    return execution_result;
  }

  return SuccessExecutionResult();
}

void AwsInstanceClientProvider::OnGetSessionTokenCallback(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context,
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_resource_name_context,
                      get_token_context.result,
                      "Failed to get the access token.");
    get_resource_name_context.result = get_token_context.result;
    get_resource_name_context.Finish();
    return;
  }

  auto signed_request = make_shared<HttpRequest>();
  signed_request->path = make_shared<string>(kAwsInstanceDynamicDataUrl);
  signed_request->method = HttpMethod::GET;

  const auto& access_token = *get_token_context.response->session_token;
  signed_request->headers = make_shared<core::HttpHeaders>();
  signed_request->headers->insert(
      {string(kAuthorizationHeaderKey), access_token});

  AsyncContext<HttpRequest, HttpResponse> http_context(
      move(signed_request),
      bind(&AwsInstanceClientProvider::OnGetInstanceResourceNameCallback, this,
           get_resource_name_context, _1),
      get_resource_name_context);

  auto execution_result = http1_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_resource_name_context, execution_result,
        "Failed to perform http request to get the current instance "
        "resource id.");
    get_resource_name_context.result = execution_result;
    get_resource_name_context.Finish();
    return;
  }
}

void AwsInstanceClientProvider::OnGetInstanceResourceNameCallback(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_resource_name_context,
                      http_client_context.result,
                      "Failed to get the current instance resource id.");
    get_resource_name_context.result = http_client_context.result;
    get_resource_name_context.Finish();
    return;
  }

  auto malformed_failure = FailureExecutionResult(
      SC_AWS_INSTANCE_CLIENT_INSTANCE_RESOURCE_NAME_RESPONSE_MALFORMED);

  json json_response;
  try {
    json_response =
        json::parse(http_client_context.response->body.bytes->begin(),
                    http_client_context.response->body.bytes->end());
  } catch (...) {
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_resource_name_context,
        malformed_failure,
        "Received http response could not be parsed into a JSON.");
    get_resource_name_context.result = malformed_failure;
    get_resource_name_context.Finish();
    return;
  }

  if (!all_of(GetRequiredFieldsForInstanceDynamicData().first,
              GetRequiredFieldsForInstanceDynamicData().second,
              [&json_response](const char* const component) {
                return json_response.contains(component);
              })) {
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_resource_name_context,
        malformed_failure,
        "Received http response doesn't contain the required fields.");
    get_resource_name_context.result = malformed_failure;
    get_resource_name_context.Finish();
    return;
  }

  auto resource_name = absl::StrFormat(
      kAwsInstanceResourceNameFormat, json_response[kRegionKey].get<string>(),
      json_response[kAccountIdKey].get<string>(),
      json_response[kInstanceIdKey].get<string>());

  get_resource_name_context.response =
      make_shared<GetCurrentInstanceResourceNameResponse>();
  get_resource_name_context.response->set_instance_resource_name(resource_name);
  get_resource_name_context.result = SuccessExecutionResult();
  get_resource_name_context.Finish();
}

ExecutionResult AwsInstanceClientProvider::GetInstanceDetailsByResourceNameSync(
    const string& resource_name, InstanceDetails& instance_details) noexcept {
  GetInstanceDetailsByResourceNameRequest request;
  request.set_instance_resource_name(resource_name);
  GetInstanceDetailsByResourceNameResponse response;
  auto execution_result =
      CpioUtils::AsyncToSync<GetInstanceDetailsByResourceNameRequest,
                             GetInstanceDetailsByResourceNameResponse>(
          bind(&AwsInstanceClientProvider::GetInstanceDetailsByResourceName,
               this, _1),
          request, response);

  if (!execution_result.Successful()) {
    SCP_ERROR(
        kAwsInstanceClientProvider, kZeroUuid, execution_result,
        "Failed to run async function GetInstanceDetailsByResourceName for "
        "resource %s",
        request.instance_resource_name().c_str());
    return execution_result;
  }

  instance_details = move(*response.mutable_instance_details());

  return SuccessExecutionResult();
}

ExecutionResult AwsInstanceClientProvider::GetInstanceDetailsByResourceName(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_details_context) noexcept {
  AwsResourceNameDetails details;
  auto execution_result = AwsInstanceClientUtils::GetResourceNameDetails(
      get_details_context.request->instance_resource_name(), details);
  if (!execution_result.Successful()) {
    get_details_context.result = execution_result;
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_details_context,
        get_details_context.result,
        "Get tags request failed due to invalid resource name %s",
        get_details_context.request->instance_resource_name().c_str());
    get_details_context.Finish();
    return execution_result;
  }

  auto ec2_client_or = GetEC2ClientByRegion(details.region);
  if (!ec2_client_or.Successful()) {
    get_details_context.result = ec2_client_or.result();
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_details_context,
        get_details_context.result,
        "Get tags request failed to create EC2Client for resource name %s",
        get_details_context.request->instance_resource_name().c_str());
    get_details_context.Finish();
    return ec2_client_or.result();
  }

  DescribeInstancesRequest request;
  request.AddInstanceIds(details.resource_id);

  (*ec2_client_or)
      ->DescribeInstancesAsync(
          request,
          bind(&AwsInstanceClientProvider::OnDescribeInstancesAsyncCallback,
               this, get_details_context, _1, _2, _3, _4));

  return SuccessExecutionResult();
}

void AwsInstanceClientProvider::OnDescribeInstancesAsyncCallback(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>& get_details_context,
    const EC2Client*, const DescribeInstancesRequest&,
    const DescribeInstancesOutcome& outcome,
    const shared_ptr<const AsyncCallerContext>&) noexcept {
  if (!outcome.IsSuccess()) {
    const auto& error_type = outcome.GetError().GetErrorType();
    auto result = EC2ErrorConverter::ConvertEC2Error(
        error_type, outcome.GetError().GetMessage());
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_details_context, result,
        "Describe instances request failed for instance %s",
        get_details_context.request->instance_resource_name().c_str());
    FinishContext(result, get_details_context, cpu_async_executor_);
    return;
  }

  if (outcome.GetResult().GetReservations().size() != 1 ||
      outcome.GetResult().GetReservations()[0].GetInstances().size() != 1) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_INSTANCE_CLIENT_PROVIDER_DESCRIBE_INSTANCES_RESPONSE_MALFORMED);
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_details_context, execution_result,
        "Describe instances response data size doesn't match with "
        "one instance request for instance %s",
        get_details_context.request->instance_resource_name().c_str());

    FinishContext(execution_result, get_details_context, cpu_async_executor_);
    return;
  }

  const auto& target_instance =
      outcome.GetResult().GetReservations()[0].GetInstances()[0];

  get_details_context.response =
      make_shared<GetInstanceDetailsByResourceNameResponse>();
  auto* instance_details =
      get_details_context.response->mutable_instance_details();

  instance_details->set_instance_id(target_instance.GetInstanceId().c_str());

  auto* network = instance_details->add_networks();
  network->set_private_ipv4_address(
      target_instance.GetPrivateIpAddress().c_str());
  network->set_public_ipv4_address(
      target_instance.GetPublicIpAddress().c_str());

  // Extract instance labels.
  auto* labels_proto = instance_details->mutable_labels();
  for (const auto& tag : target_instance.GetTags()) {
    labels_proto->insert(MapPair(tag.GetKey(), tag.GetValue()));
  }

  FinishContext(SuccessExecutionResult(), get_details_context,
                cpu_async_executor_);
}

ExecutionResult AwsInstanceClientProvider::GetTagsByResourceName(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context) noexcept {
  AwsResourceNameDetails details;
  auto execution_result = AwsInstanceClientUtils::GetResourceNameDetails(
      get_tags_context.request->resource_name(), details);
  if (!execution_result.Successful()) {
    get_tags_context.result = execution_result;
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_tags_context,
                      get_tags_context.result,
                      "Get tags request failed due to invalid resource name %s",
                      get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish();
    return execution_result;
  }

  auto ec2_client_or = GetEC2ClientByRegion(details.region);
  if (!ec2_client_or.Successful()) {
    get_tags_context.result = ec2_client_or.result();
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_tags_context, get_tags_context.result,
        "Get tags request failed to create EC2Client for resource name %s",
        get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish();
    return ec2_client_or.result();
  }

  DescribeTagsRequest request;

  Filter resource_name_filter;
  resource_name_filter.SetName(kResourceIdFilterName);
  resource_name_filter.AddValues(details.resource_id);
  request.AddFilters(resource_name_filter);

  (*ec2_client_or)
      ->DescribeTagsAsync(
          request, bind(&AwsInstanceClientProvider::OnDescribeTagsAsyncCallback,
                        this, get_tags_context, _1, _2, _3, _4));
  return SuccessExecutionResult();
}

void AwsInstanceClientProvider::OnDescribeTagsAsyncCallback(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context,
    const EC2Client*, const DescribeTagsRequest&,
    const DescribeTagsOutcome& outcome,
    const shared_ptr<const AsyncCallerContext>&) noexcept {
  if (!outcome.IsSuccess()) {
    const auto& error_type = outcome.GetError().GetErrorType();
    auto result = EC2ErrorConverter::ConvertEC2Error(
        error_type, outcome.GetError().GetMessage());
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_tags_context, result,
                      "Get tags request failed for resource %s",
                      get_tags_context.request->resource_name().c_str());
    FinishContext(result, get_tags_context, cpu_async_executor_);
    return;
  }

  get_tags_context.response = make_shared<GetTagsByResourceNameResponse>();
  auto* tags = get_tags_context.response->mutable_tags();

  for (const auto& tag : outcome.GetResult().GetTags()) {
    tags->insert(MapPair(tag.GetKey(), tag.GetValue()));
  }

  FinishContext(SuccessExecutionResult(), get_tags_context,
                cpu_async_executor_);
}

ExecutionResultOr<shared_ptr<EC2Client>> AwsEC2ClientFactory::CreateClient(
    const string& region,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  auto client_config =
      common::CreateClientConfiguration(make_shared<string>(region));
  client_config->maxConnections = kMaxConcurrentConnections;
  client_config->executor = make_shared<AwsAsyncExecutor>(io_async_executor);
  return make_shared<EC2Client>(*client_config);
}

shared_ptr<InstanceClientProviderInterface>
InstanceClientProviderFactory::Create(
    const shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
    const shared_ptr<HttpClientInterface>& http1_client,
    const shared_ptr<HttpClientInterface>& http2_client,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return make_shared<AwsInstanceClientProvider>(
      auth_token_provider, http1_client, cpu_async_executor, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
