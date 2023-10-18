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
#include <utility>
#include <vector>

#include <aws/ec2/EC2Client.h>

#include "cc/core/common/concurrent_map/src/concurrent_map.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {

class AwsEC2ClientFactory;

/*! @copydoc InstanceClientProviderInterface
 */
class AwsInstanceClientProvider : public InstanceClientProviderInterface {
 public:
  /// Constructs a new Aws Instance Client Provider object
  AwsInstanceClientProvider(
      const std::shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
      const std::shared_ptr<core::HttpClientInterface>& http1_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<AwsEC2ClientFactory>& ec2_factory =
          std::make_shared<AwsEC2ClientFactory>())
      : auth_token_provider_(auth_token_provider),
        http1_client_(http1_client),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor),
        ec2_factory_(ec2_factory) {}

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
  /**
   * @brief Is called after auth_token_provider GetSessionToken() for session
   * token is completed
   *
   * @param get_resource_name_context the context for getting current instance
   * resource id.
   * @param get_token_context the context of get session token.
   */
  void OnGetSessionTokenCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          get_resource_name_context,
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_token_context) noexcept;

  /**
   * @brief Is called after http client PerformRequest() for current instance
   * id is completed.
   *
   * @param get_resource_name_context the context for getting current instance
   * resource id.
   * @param http_client_context the context for http_client PerformRequest().
   */
  void OnGetInstanceResourceNameCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          get_resource_name_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  /**
   * @brief Is called after ec2_client DescribeInstancesAsync() for a given
   * instance is completed.
   *
   * @param get_details_context the context to get the details of a instance.
   * @param outcome the outcome of DescribeInstancesAsync().
   */
  void OnDescribeInstancesAsyncCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          get_details_context,
      const Aws::EC2::EC2Client*,
      const Aws::EC2::Model::DescribeInstancesRequest&,
      const Aws::EC2::Model::DescribeInstancesOutcome& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) noexcept;

  /**
   * @brief Is called after ec2_client DescribeTagsAsync() for a given
   * resource id is completed.
   *
   * @param get_tags_context the context to get the tags of a given resource id.
   * @param outcome the outcome of DescribeTagsAsync().
   */
  void OnDescribeTagsAsyncCallback(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          get_tags_context,
      const Aws::EC2::EC2Client*, const Aws::EC2::Model::DescribeTagsRequest&,
      const Aws::EC2::Model::DescribeTagsOutcome& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) noexcept;

  /**
   * @brief Get EC2 clients from ec2_clients_list_ by region code. If this EC2
   * client does not exist, create a new one and store it in ec2_clients_list_.
   *
   * @param region AWS region code. If the region is an empty string, the
   * default region code `us-east-1` will be applied.
   * @return core::ExecutionResultOr<std::shared_ptr<Aws::EC2::EC2Client>> EC2
   * client if success.
   */
  core::ExecutionResultOr<std::shared_ptr<Aws::EC2::EC2Client>>
  GetEC2ClientByRegion(const std::string& region) noexcept;

  /// On-demand EC2 client for region codes.
  core::common::ConcurrentMap<std::string, std::shared_ptr<Aws::EC2::EC2Client>>
      ec2_clients_list_;

  /// Instance of auth token provider.
  std::shared_ptr<AuthTokenProviderInterface> auth_token_provider_;
  /// Instance of http client.
  std::shared_ptr<core::HttpClientInterface> http1_client_;
  /// Instances of the async executor for local compute and blocking IO
  /// operations respectively.
  const std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;

  /// An instance of the factory for Aws::EC2::EC2Client.
  std::shared_ptr<AwsEC2ClientFactory> ec2_factory_;
};

/// Creates Aws::EC2::EC2Client
class AwsEC2ClientFactory {
 public:
  virtual core::ExecutionResultOr<std::shared_ptr<Aws::EC2::EC2Client>>
  CreateClient(const std::string& region,
               const std::shared_ptr<core::AsyncExecutorInterface>&
                   io_async_executor) noexcept;

  virtual ~AwsEC2ClientFactory() = default;
};

}  // namespace google::scp::cpio::client_providers
