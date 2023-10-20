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

#include "gcp_instance_client_provider.h"

#include <future>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "cpio/common/src/cpio_utils.h"

#include "error_codes.h"
#include "gcp_instance_client_utils.h"

using absl::StrCat;
using absl::StrFormat;
using absl::StrSplit;
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
using google::protobuf::MapPair;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_TYPE;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_PROVIDER_SERVICE_UNAVAILABLE;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED;
using google::scp::core::errors::SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::common::CpioUtils;
using nlohmann::json;
using std::all_of;
using std::atomic;
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
using std::nullopt;
using std::optional;
using std::pair;
using std::promise;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;

namespace {
constexpr char kGcpInstanceClientProvider[] = "GcpInstanceClientProvider";

constexpr char kURIForInstancePrivateIpv4Address[] =
    "http://metadata.google.internal/computeMetadata/v1/instance/"
    "network-interfaces/0/ip";
constexpr char kURIForInstanceId[] =
    "http://metadata.google.internal/computeMetadata/v1/instance/id";
constexpr char kURIForInstanceZone[] =
    "http://metadata.google.internal/computeMetadata/v1/instance/zone";
constexpr char kURIForProjectId[] =
    "http://metadata.google.internal/computeMetadata/v1/project/project-id";
constexpr char kMetadataFlavorHeaderKey[] = "Metadata-Flavor";
constexpr char kMetadataFlavorHeaderValue[] = "Google";

constexpr char kGcpInstanceRNFormatString[] =
    "//compute.googleapis.com/projects/%s/zones/%s/instances/%s";

constexpr char kInstanceResourceNamePrefix[] = "//compute.googleapis.com/";
constexpr char kGcpInstanceGetUrlPrefix[] =
    "https://compute.googleapis.com/compute/v1/";
constexpr char kAuthorizationHeaderKey[] = "Authorization";
constexpr char kBearerTokenPrefix[] = "Bearer ";
constexpr char kInstanceDetailsJsonIdKey[] = "id";
constexpr char kNetworkInterfacesKey[] = "networkInterfaces";
constexpr char kPrivateIpKey[] = "networkIP";
constexpr char kAccessConfigs[] = "accessConfigs";
constexpr char kPublicIpKey[] = "natIP";
constexpr char kInstanceLabelsKey[] = "labels";

constexpr char kParentParameter[] = "parent=";

// The server allows a maximum of 300 TagBindings to return.
// For more information, see
// https://cloud.google.com/resource-manager/reference/rest/v3/tagBindings/list
constexpr char kPageSizeSetting[] = "pageSize=300";
constexpr char kTagBindingNameKey[] = "name";
constexpr char kTagBindingParentKey[] = "parent";
constexpr char kTagBindingTagValueKey[] = "tagValue";
constexpr char kTagBindingsListKey[] = "tagBindings";

// Returns a pair of iterators - one to the beginning, one to the end.
const auto& GetRequiredFieldsForInstanceDetails() {
  static char const* components[2];
  using iterator_type = decltype(cbegin(components));
  static pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kInstanceDetailsJsonIdKey;
    components[1] = kNetworkInterfacesKey;
    return make_pair(cbegin(components), cend(components));
  }();
  return iterator_pair;
}

const auto& GetRequiredFieldsForResourceTags() {
  static char const* components[3];
  using iterator_type = decltype(cbegin(components));
  static pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kTagBindingNameKey;
    components[1] = kTagBindingParentKey;
    components[2] = kTagBindingTagValueKey;
    return make_pair(cbegin(components), cend(components));
  }();
  return iterator_pair;
}

}  // namespace

namespace google::scp::cpio::client_providers {

GcpInstanceClientProvider::GcpInstanceClientProvider(
    const shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
    const shared_ptr<HttpClientInterface>& http1_client,
    const shared_ptr<HttpClientInterface>& http2_client)
    : http1_client_(http1_client),
      http2_client_(http2_client),
      auth_token_provider_(auth_token_provider),
      http_uri_instance_private_ipv4_(
          make_shared<std::string>(kURIForInstancePrivateIpv4Address)),
      http_uri_instance_id_(make_shared<std::string>(kURIForInstanceId)),
      http_uri_project_id_(make_shared<std::string>(kURIForProjectId)),
      http_uri_instance_zone_(make_shared<std::string>(kURIForInstanceZone)) {}

ExecutionResult GcpInstanceClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    string& resource_name) noexcept {
  GetCurrentInstanceResourceNameRequest request;
  GetCurrentInstanceResourceNameResponse response;
  auto execution_result =
      CpioUtils::AsyncToSync<GetCurrentInstanceResourceNameRequest,
                             GetCurrentInstanceResourceNameResponse>(
          bind(&GcpInstanceClientProvider::GetCurrentInstanceResourceName, this,
               _1),
          request, response);

  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpInstanceClientProvider, kZeroUuid, execution_result,
              "Failed to run async function GetCurrentInstanceResourceName for "
              "current instance resource name");
    return execution_result;
  }

  resource_name = move(*response.mutable_instance_resource_name());

  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceClientProvider::GetCurrentInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context) noexcept {
  auto instance_resource_name_tracker =
      make_shared<InstanceResourceNameTracker>();

  auto execution_result = MakeHttpRequestsForInstanceResourceName(
      get_resource_name_context, http_uri_project_id_,
      instance_resource_name_tracker, ResourceType::kProjectId);
  RETURN_IF_FAILURE(execution_result);

  execution_result = MakeHttpRequestsForInstanceResourceName(
      get_resource_name_context, http_uri_instance_zone_,
      instance_resource_name_tracker, ResourceType::kZone);
  RETURN_IF_FAILURE(execution_result);

  execution_result = MakeHttpRequestsForInstanceResourceName(
      get_resource_name_context, http_uri_instance_id_,
      instance_resource_name_tracker, ResourceType::kInstanceId);
  RETURN_IF_FAILURE(execution_result);

  return SuccessExecutionResult();
}

ExecutionResult
GcpInstanceClientProvider::MakeHttpRequestsForInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context,
    shared_ptr<string>& uri,
    shared_ptr<InstanceResourceNameTracker> instance_resource_name_tracker,
    ResourceType type) noexcept {
  auto http_request = make_shared<HttpRequest>();
  http_request->method = HttpMethod::GET;
  http_request->path = uri;
  http_request->body.length = 0;
  http_request->headers = make_shared<core::HttpHeaders>();
  http_request->headers->insert(
      {string(kMetadataFlavorHeaderKey), string(kMetadataFlavorHeaderValue)});

  AsyncContext<HttpRequest, HttpResponse> http_context(
      move(http_request),
      bind(&GcpInstanceClientProvider::OnGetInstanceResourceName, this,
           get_resource_name_context, _1, instance_resource_name_tracker, type),
      get_resource_name_context);

  auto execution_result = http1_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    // If got_failure exchange success, then the other thread hasn't failed -
    // we should be the ones to log and finish the context.
    auto got_result = false;
    if (instance_resource_name_tracker->got_failure.compare_exchange_strong(
            got_result, true)) {
      get_resource_name_context.result = execution_result;
      SCP_ERROR_CONTEXT(
          kGcpInstanceClientProvider, get_resource_name_context,
          get_resource_name_context.result,
          "Failed to perform http request to fetch resource id with uri %s",
          uri->c_str());
      get_resource_name_context.Finish();
    }
    return execution_result;
  }

  return SuccessExecutionResult();
}

void GcpInstanceClientProvider::OnGetInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context,
    shared_ptr<InstanceResourceNameTracker> instance_resource_name_tracker,
    ResourceType type) noexcept {
  // If got_failure is true, no need to process this request.
  if (instance_resource_name_tracker->got_failure.load()) {
    return;
  }

  auto result = http_client_context.result;
  if (!result.Successful()) {
    // If got_failure exchange success, then the other thread hasn't failed -
    // we should be the ones to log and finish the context.
    auto got_result = false;
    if (instance_resource_name_tracker->got_failure.compare_exchange_strong(
            got_result, true)) {
      get_resource_name_context.result = result;
      SCP_ERROR_CONTEXT(
          kGcpInstanceClientProvider, get_resource_name_context,
          get_resource_name_context.result,
          "Failed to perform http request to fetch resource id with uri %s",
          http_client_context.request->path->c_str());
      get_resource_name_context.Finish();
    }
    return;
  }

  switch (type) {
    case ResourceType::kProjectId: {
      instance_resource_name_tracker->project_id =
          http_client_context.response->body.ToString();
      break;
    }
    case ResourceType::kInstanceId: {
      instance_resource_name_tracker->instance_id =
          http_client_context.response->body.ToString();
      break;
    }
    case ResourceType::kZone: {
      // The response from zone fetching is
      // `projects/PROJECT_NUMBER/zones/ZONE_ID`, and the project number is
      // different from project ID. In some cases, the project number doesn't
      // work. (e.g, in the spanner, using project number will have permission
      // issue)
      vector<string> splits =
          absl::StrSplit(http_client_context.response->body.ToString(), "/");
      instance_resource_name_tracker->instance_zone = move(splits.back());
      break;
    }
    default: {
      get_resource_name_context.result = FailureExecutionResult(
          SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_TYPE);
      SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_resource_name_context,
                        get_resource_name_context.result,
                        "Invalid instance resource type %d.", type);
      get_resource_name_context.Finish();
    }
  }

  auto prev_unfinished =
      instance_resource_name_tracker->num_outstanding_calls.fetch_sub(1);
  if (prev_unfinished == 1) {
    get_resource_name_context.response =
        make_shared<GetCurrentInstanceResourceNameResponse>();
    // The instance resource name is
    // `projects/PROJECT_ID/zones/ZONE_ID/instances/INSTANCE_ID`.
    auto resource_name = StrFormat(
        kGcpInstanceRNFormatString, instance_resource_name_tracker->project_id,
        instance_resource_name_tracker->instance_zone,
        instance_resource_name_tracker->instance_id);
    get_resource_name_context.response->set_instance_resource_name(
        resource_name);
    get_resource_name_context.result = SuccessExecutionResult();
    get_resource_name_context.Finish();
  }
}

ExecutionResult GcpInstanceClientProvider::GetTagsByResourceName(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context) noexcept {
  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(
          make_shared<GetSessionTokenRequest>(),
          bind(&GcpInstanceClientProvider::OnGetSessionTokenForTagsCallback,
               this, get_tags_context, _1),
          get_tags_context);

  auto execution_result =
      auth_token_provider_->GetSessionToken(get_token_context);
  if (!execution_result.Successful()) {
    get_tags_context.result = execution_result;
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_tags_context,
                      get_tags_context.result,
                      "Failed to get the tags for resource %s",
                      get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish();

    return execution_result;
  }

  return SuccessExecutionResult();
}

void GcpInstanceClientProvider::OnGetSessionTokenForTagsCallback(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context,
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_tags_context,
                      get_token_context.result,
                      "Failed to get the access token for resource %s",
                      get_tags_context.request->resource_name().c_str());
    get_tags_context.result = get_token_context.result;
    get_tags_context.Finish();
    return;
  }

  auto uri = GcpInstanceClientUtils::CreateRMListTagsUrl(
      get_tags_context.request->resource_name());
  auto signed_request = make_shared<HttpRequest>();
  signed_request->path = make_shared<string>(move(uri));
  signed_request->method = HttpMethod::GET;
  signed_request->query = make_shared<string>(absl::StrCat(
      kParentParameter, get_tags_context.request->resource_name().c_str(), "&",
      kPageSizeSetting));

  const auto& access_token = *get_token_context.response->session_token;
  signed_request->headers = make_shared<core::HttpHeaders>();
  signed_request->headers->insert(
      {string(kAuthorizationHeaderKey),
       absl::StrCat(kBearerTokenPrefix, access_token)});

  AsyncContext<HttpRequest, HttpResponse> http_context(
      move(signed_request),
      bind(&GcpInstanceClientProvider::OnGetTagsByResourceNameCallback, this,
           get_tags_context, _1),
      get_tags_context);

  auto execution_result = http2_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    get_tags_context.result = execution_result;
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_tags_context,
                      get_tags_context.result,
                      "Failed to perform http request to get the tags "
                      "of resource %s",
                      get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish();
    return;
  }
}

void GcpInstanceClientProvider::OnGetTagsByResourceNameCallback(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_tags_context,
        http_client_context.result,
        "Failed to perform http request to get the tags for resource %s",
        get_tags_context.request->resource_name().c_str());
    get_tags_context.result = http_client_context.result;
    get_tags_context.Finish();
    return;
  }

  auto malformed_failure = FailureExecutionResult(
      SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED);

  json json_response;
  try {
    json_response =
        json::parse(http_client_context.response->body.bytes->begin(),
                    http_client_context.response->body.bytes->end());
  } catch (...) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_tags_context, malformed_failure,
        "Received http response could not be parsed into a JSON for "
        "resource %s",
        get_tags_context.request->resource_name().c_str());
    get_tags_context.result = malformed_failure;
    get_tags_context.Finish();
    return;
  }

  // If json_response is empty, return success with empty response to avoid
  // falling into below malformed checks.
  if (json_response.empty()) {
    get_tags_context.response = make_shared<GetTagsByResourceNameResponse>();
    get_tags_context.result = SuccessExecutionResult();
    get_tags_context.Finish();
    return;
  }

  if (!json_response.contains(kTagBindingsListKey)) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_tags_context, malformed_failure,
        "Received http response doesn't contain the required fields "
        "for resource %s",
        get_tags_context.request->resource_name().c_str());
    get_tags_context.result = malformed_failure;
    get_tags_context.Finish();
    return;
  }

  for (const auto& tag_binding : json_response[kTagBindingsListKey]) {
    if (!all_of(GetRequiredFieldsForResourceTags().first,
                GetRequiredFieldsForResourceTags().second,
                [&tag_binding](const char* const component) {
                  return tag_binding.contains(component);
                })) {
      SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_tags_context,
                        malformed_failure,
                        "Received http response doesn't contain the required "
                        "fields for resource %s",
                        get_tags_context.request->resource_name().c_str());
      get_tags_context.result = malformed_failure;
      get_tags_context.Finish();
      return;
    }
  }

  get_tags_context.response = make_shared<GetTagsByResourceNameResponse>();
  auto* tags = get_tags_context.response->mutable_tags();

  for (const auto& tag : json_response[kTagBindingsListKey]) {
    tags->insert(MapPair(tag[kTagBindingNameKey].get<string>(),
                         tag[kTagBindingTagValueKey].get<string>()));
  }

  get_tags_context.result = SuccessExecutionResult();
  get_tags_context.Finish();
}

ExecutionResult GcpInstanceClientProvider::GetInstanceDetailsByResourceNameSync(
    const std::string& resource_name,
    cmrt::sdk::instance_service::v1::InstanceDetails&
        instance_details) noexcept {
  GetInstanceDetailsByResourceNameRequest request;
  request.set_instance_resource_name(resource_name);
  GetInstanceDetailsByResourceNameResponse response;
  auto execution_result =
      CpioUtils::AsyncToSync<GetInstanceDetailsByResourceNameRequest,
                             GetInstanceDetailsByResourceNameResponse>(
          bind(&GcpInstanceClientProvider::GetInstanceDetailsByResourceName,
               this, _1),
          request, response);

  if (!execution_result.Successful()) {
    SCP_ERROR(
        kGcpInstanceClientProvider, kZeroUuid, execution_result,
        "Failed to run async function GetInstanceDetailsByResourceName for "
        "resource %s",
        request.instance_resource_name().c_str());
    return execution_result;
  }

  instance_details = move(*response.mutable_instance_details());

  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceClientProvider::GetInstanceDetailsByResourceName(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_instance_details_context) noexcept {
  auto execution_result =
      GcpInstanceClientUtils::ValidateInstanceResourceNameFormat(
          get_instance_details_context.request->instance_resource_name());
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        execution_result,
        "Failed to parse instance resource ID from instance resource name %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_instance_details_context.result = execution_result;
    get_instance_details_context.Finish();
    return execution_result;
  }

  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(make_shared<GetSessionTokenRequest>(),
                        bind(&GcpInstanceClientProvider::
                                 OnGetSessionTokenForInstanceDetailsCallback,
                             this, get_instance_details_context, _1),
                        get_instance_details_context);

  execution_result = auth_token_provider_->GetSessionToken(get_token_context);
  if (!execution_result.Successful()) {
    get_instance_details_context.result = execution_result;
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        get_instance_details_context.result,
        "Failed to perform http request to get the details "
        "of instance %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_instance_details_context.Finish();

    return execution_result;
  }

  return SuccessExecutionResult();
}

void GcpInstanceClientProvider::OnGetSessionTokenForInstanceDetailsCallback(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_instance_details_context,
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_instance_details_context,
                      get_token_context.result,
                      "Failed to get the access token.");
    get_instance_details_context.result = get_token_context.result;
    get_instance_details_context.Finish();
    return;
  }

  auto resource_id =
      get_instance_details_context.request->instance_resource_name().substr(
          strlen(kInstanceResourceNamePrefix));

  auto uri = absl::StrCat(kGcpInstanceGetUrlPrefix, resource_id);
  auto signed_request = make_shared<HttpRequest>();
  signed_request->path = make_shared<string>(move(uri));
  signed_request->method = HttpMethod::GET;

  const auto& access_token = *get_token_context.response->session_token;
  signed_request->headers = make_shared<core::HttpHeaders>();
  signed_request->headers->insert(
      {string(kAuthorizationHeaderKey),
       absl::StrCat(kBearerTokenPrefix, access_token)});

  AsyncContext<HttpRequest, HttpResponse> http_context(
      move(signed_request),
      bind(&GcpInstanceClientProvider::OnGetInstanceDetailsCallback, this,
           get_instance_details_context, _1),
      get_instance_details_context);

  auto execution_result = http2_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    get_instance_details_context.result = execution_result;
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        get_instance_details_context.result,
        "Failed to perform http request to get the details "
        "of instance %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_instance_details_context.Finish();
    return;
  }
}

void GcpInstanceClientProvider::OnGetInstanceDetailsCallback(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_instance_details_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        http_client_context.result,
        "Failed to perform http request to get the details "
        "of instance %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_instance_details_context.result = http_client_context.result;
    get_instance_details_context.Finish();
    return;
  }

  json json_response;
  try {
    json_response =
        json::parse(http_client_context.response->body.bytes->begin(),
                    http_client_context.response->body.bytes->end());
  } catch (...) {
    auto result = FailureExecutionResult(
        SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED);
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context, result,
        "Received http response could not be parsed into a JSON.");
    get_instance_details_context.result = result;
    get_instance_details_context.Finish();
    return;
  }

  if (!all_of(GetRequiredFieldsForInstanceDetails().first,
              GetRequiredFieldsForInstanceDetails().second,
              [&json_response](const char* const component) {
                return json_response.contains(component);
              })) {
    auto result = FailureExecutionResult(
        SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED);
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context, result,
        "Received http response doesn't contain the required fields.");
    get_instance_details_context.result = result;
    get_instance_details_context.Finish();
    return;
  }

  get_instance_details_context.response =
      make_shared<GetInstanceDetailsByResourceNameResponse>();
  auto* instance_details =
      get_instance_details_context.response->mutable_instance_details();

  auto instance_id = json_response[kInstanceDetailsJsonIdKey].get<string>();
  instance_details->set_instance_id(move(instance_id));

  // Get instance networks info from networkInterfaces.
  for (const auto& network_interface : json_response[kNetworkInterfacesKey]) {
    string private_ip, public_ip;
    if (network_interface.contains(kPrivateIpKey)) {
      private_ip = network_interface[kPrivateIpKey].get<string>();
    }

    // Current GCP only supports one accessConfig.
    if (network_interface.contains(kAccessConfigs)) {
      for (const auto& access_config : network_interface[kAccessConfigs]) {
        if (access_config.contains(kPublicIpKey)) {
          public_ip = access_config[kPublicIpKey].get<string>();
          break;
        }
      }
    }

    auto* network = instance_details->add_networks();
    network->set_private_ipv4_address(move(private_ip));
    network->set_public_ipv4_address(move(public_ip));
  }

  // Extract instance labels.
  auto labels = json_response.find(kInstanceLabelsKey);
  if (labels != json_response.end()) {
    auto* labels_proto =
        get_instance_details_context.response->mutable_instance_details()
            ->mutable_labels();
    for (json::iterator it = labels->begin(); it != labels->end(); ++it) {
      labels_proto->insert(MapPair(it.key(), it.value().get<string>()));
    }
  }

  get_instance_details_context.result = SuccessExecutionResult();
  get_instance_details_context.Finish();
}

shared_ptr<InstanceClientProviderInterface>
InstanceClientProviderFactory::Create(
    const shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
    const shared_ptr<HttpClientInterface>& http1_client,
    const shared_ptr<HttpClientInterface>& http2_client,
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpInstanceClientProvider>(auth_token_provider,
                                                http1_client, http2_client);
}

}  // namespace google::scp::cpio::client_providers
