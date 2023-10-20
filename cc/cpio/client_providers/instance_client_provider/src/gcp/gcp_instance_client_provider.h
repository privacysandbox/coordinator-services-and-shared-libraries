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

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/http_client_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"

namespace google::scp::cpio::client_providers {
class GcpInstanceClientProvider : public InstanceClientProviderInterface {
 public:
  GcpInstanceClientProvider(
      const std::shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
      const std::shared_ptr<core::HttpClientInterface>& http1_client,
      const std::shared_ptr<core::HttpClientInterface>& http2_client);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetCurrentInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          context) noexcept override;

  core::ExecutionResult GetTagsByResourceName(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          context) noexcept override;

  core::ExecutionResult GetInstanceDetailsByResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          context) noexcept override;

  core::ExecutionResult GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override;

  core::ExecutionResult GetInstanceDetailsByResourceNameSync(
      const std::string& resource_name,
      cmrt::sdk::instance_service::v1::InstanceDetails&
          instance_details) noexcept override;

 private:
  // The tracker for instance resource id fetching status.
  struct InstanceResourceNameTracker {
    // Project ID fetching response.
    std::string project_id;
    // Instance zone fetching response.
    std::string instance_zone;
    // Instance id fetching response.
    std::string instance_id;
    // Num of outstanding calls left.
    std::atomic<size_t> num_outstanding_calls{3};
    // Whether get_resource_name_context got failure result
    std::atomic<bool> got_failure{false};
  };

  enum class ResourceType {
    kZone = 1,
    kInstanceId = 2,
    kProjectId = 3,
  };

  /**
   * @brief make http client request for resource id fetching.
   *
   * @param get_resource_name_context get current instance resource id context
   * @param uri uri for http client PerformRequest()
   * @param instance_resource_name_tracker the tracker for instance resource id
   * fetching.
   * @param type the type of the resource data type requested.
   * @return core::ExecutionResult
   */
  core::ExecutionResult MakeHttpRequestsForInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          get_resource_name_context,
      std::shared_ptr<std::string>& uri,
      std::shared_ptr<InstanceResourceNameTracker>
          instance_resource_name_tracker,
      ResourceType type) noexcept;

  /**
   * @brief Is called after http client PerformRequest() for resource id is
   * completed.
   *
   * @param get_resource_name_context get current instance resource id context
   * @param http_client_context http client PerformRequest() context
   * @param instance_resource_name_tracker the tracker for instance resource id
   * fetching.
   * @param type the type of the resource data type requested.
   */
  void OnGetInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          get_resource_name_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context,
      std::shared_ptr<InstanceResourceNameTracker>
          instance_resource_name_tracker,
      ResourceType type) noexcept;

  /**
   * @brief Is called after auth_token_provider GetSessionToken() for session
   * token is completed
   *
   * @param get_instance_details_context the context for getting instance
   * details.
   * @param get_session_token the context of get session token.
   */
  void OnGetSessionTokenForInstanceDetailsCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          get_instance_details_context,
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_session_token) noexcept;

  /**
   * @brief Is called after http client PerformRequest() is completed.
   *
   * @param get_instance_details_context the context for getting instance.
   * details.
   * @param http_client_context the context for http_client PerformRequest().
   */
  void OnGetInstanceDetailsCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          get_instance_details_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  /**
   * @brief Is called after auth_token_provider GetSessionToken() for session
   * token is completed
   *
   * @param get_tags_context the context for getting the tags for a
   * given resource.
   * @param get_session_token the context of get session token.
   */
  void OnGetSessionTokenForTagsCallback(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          get_tags_context,
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_session_token) noexcept;

  /**
   * @brief Is called after http client PerformRequest() is completed.
   *
   * @param get_tags_context the context for getting the tags for a
   * given resource.
   * @param http_client_context the context for http_client PerformRequest().
   */
  void OnGetTagsByResourceNameCallback(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          get_tags_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  /// Instance of http1 client.
  std::shared_ptr<core::HttpClientInterface> http1_client_;
  /// Instance of http2 client.
  std::shared_ptr<core::HttpClientInterface> http2_client_;
  /// Instance of auth token provider.
  std::shared_ptr<AuthTokenProviderInterface> auth_token_provider_;

  // Reuse shared_ptr strings across HTTP requests issued by this class
  std::shared_ptr<std::string> http_uri_instance_private_ipv4_;
  std::shared_ptr<std::string> http_uri_instance_id_;
  std::shared_ptr<std::string> http_uri_project_id_;
  std::shared_ptr<std::string> http_uri_instance_zone_;
};
}  // namespace google::scp::cpio::client_providers
