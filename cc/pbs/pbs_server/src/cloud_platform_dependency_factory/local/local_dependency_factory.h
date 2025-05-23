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

#include "cc/pbs/interface/cloud_platform_dependency_factory_interface.h"

namespace privacy_sandbox::pbs {

class LocalDependencyFactory : public CloudPlatformDependencyFactoryInterface {
 public:
  LocalDependencyFactory(
      const std::shared_ptr<pbs_common::ConfigProviderInterface>&
          config_provider)
      : config_provider_(config_provider) {}

  pbs_common::ExecutionResult Init() noexcept override;

  std::unique_ptr<pbs_common::AuthorizationProxyInterface>
  ConstructAuthorizationProxyClient(
      std::shared_ptr<pbs_common::AsyncExecutorInterface> async_executor,
      std::shared_ptr<pbs_common::HttpClientInterface> http_client) noexcept
      override;

  std::unique_ptr<pbs_common::AuthorizationProxyInterface>
  ConstructAwsAuthorizationProxyClient(
      std::shared_ptr<pbs_common::AsyncExecutorInterface> async_executor,
      std::shared_ptr<pbs_common::HttpClientInterface> http_client) noexcept
      override;

  std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
  ConstructBudgetConsumptionHelper(
      pbs_common::AsyncExecutorInterface* async_executor,
      pbs_common::AsyncExecutorInterface* io_async_executor) noexcept override;

  std::unique_ptr<pbs_common::MetricRouter> ConstructMetricRouter() noexcept
      override;

 private:
  pbs_common::ExecutionResult ReadConfigurations();

  std::shared_ptr<pbs_common::ConfigProviderInterface> config_provider_;
};

}  // namespace privacy_sandbox::pbs
