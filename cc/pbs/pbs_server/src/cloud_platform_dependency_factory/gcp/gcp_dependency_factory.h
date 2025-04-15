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

#include <memory>
#include <string>

#include "cc/core/interface/async_executor_interface.h"
#include "cc/pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace google::scp::pbs {

class GcpDependencyFactory : public CloudPlatformDependencyFactoryInterface {
 public:
  GcpDependencyFactory(
      std::shared_ptr<privacy_sandbox::pbs_common::ConfigProviderInterface>
          config_provider);

  core::ExecutionResult Init() noexcept override;

  std::unique_ptr<privacy_sandbox::pbs_common::AuthorizationProxyInterface>
  ConstructAuthorizationProxyClient(
      std::shared_ptr<privacy_sandbox::pbs_common::AsyncExecutorInterface>
          async_executor,
      std::shared_ptr<privacy_sandbox::pbs_common::HttpClientInterface>
          http_client) noexcept override;

  std::unique_ptr<privacy_sandbox::pbs_common::AuthorizationProxyInterface>
  ConstructAwsAuthorizationProxyClient(
      std::shared_ptr<privacy_sandbox::pbs_common::AsyncExecutorInterface>
          async_executor,
      std::shared_ptr<privacy_sandbox::pbs_common::HttpClientInterface>
          http_client) noexcept override;

  std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
  ConstructBudgetConsumptionHelper(
      privacy_sandbox::pbs_common::AsyncExecutorInterface* async_executor,
      privacy_sandbox::pbs_common::AsyncExecutorInterface*
          io_async_executor) noexcept override;

  std::unique_ptr<core::MetricRouter> ConstructMetricRouter() noexcept override;

  // This overload exists so that we can pass in a Resource from either this
  // dependency factory, or an integration test.
  std::unique_ptr<core::MetricRouter> ConstructMetricRouter(
      opentelemetry::sdk::resource::Resource resource) noexcept;

 protected:
  core::ExecutionResult ReadConfigurations();

  std::shared_ptr<privacy_sandbox::pbs_common::ConfigProviderInterface>
      config_provider_;

  // Configurations here
  std::string budget_key_table_name_;
  std::string auth_service_endpoint_;
  std::string alternate_auth_service_endpoint_;
  std::string alternate_cloud_service_region_;
};

}  // namespace google::scp::pbs
