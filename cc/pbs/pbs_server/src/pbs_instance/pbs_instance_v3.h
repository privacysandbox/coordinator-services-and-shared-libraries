// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CC_PBS_PBS_SERVER_SRC_PBS_INSTANCE_PBS_INSTANCE_V3
#define CC_PBS_PBS_SERVER_SRC_PBS_INSTANCE_PBS_INSTANCE_V3

#include <memory>

#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_client_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/interface/service_interface.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cc/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cc/pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance_configuration.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::pbs {

class PBSInstanceV3 : public core::ServiceInterface {
 public:
  PBSInstanceV3(std::shared_ptr<core::ConfigProviderInterface> config_provider,
                std::unique_ptr<CloudPlatformDependencyFactoryInterface>
                    cloud_platform_dependency_factory);

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

 private:
  core::ExecutionResult CreateComponents() noexcept;

  std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor_;
  std::shared_ptr<core::HttpClientInterface> http1_client_;
  std::shared_ptr<core::HttpClientInterface> http2_client_;
  std::shared_ptr<core::AuthorizationProxyInterface> authorization_proxy_;
  std::shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
      auth_token_provider_;
  std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
      instance_client_provider_;
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;
  std::shared_ptr<core::AuthorizationProxyInterface>
      pass_thru_authorization_proxy_;
  std::shared_ptr<core::HttpServerInterface> http_server_;
  std::shared_ptr<core::HttpServerInterface> health_http_server_;
  std::shared_ptr<core::ServiceInterface> health_service_;
  std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
      budget_consumption_helper_;
  std::shared_ptr<pbs::FrontEndServiceInterface> front_end_service_;

  PBSInstanceConfig pbs_instance_config_;

  std::unique_ptr<CloudPlatformDependencyFactoryInterface>
      cloud_platform_dependency_factory_;

  std::shared_ptr<core::MetricRouter> metric_router_;
};

}  // namespace google::scp::pbs

#endif  // CC_PBS_PBS_SERVER_SRC_PBS_INSTANCE_PBS_INSTANCE_V3
