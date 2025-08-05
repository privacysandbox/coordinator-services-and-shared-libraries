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

#ifndef CC_PBS_PBS_SERVER_SRC_PBS_INSTANCE_PBS_INSTANCE_V3_H_
#define CC_PBS_PBS_SERVER_SRC_PBS_INSTANCE_PBS_INSTANCE_V3_H_

#include <memory>
#include <string>

#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_client_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/interface/service_interface.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance_configuration.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs {

class PBSInstanceV3 : public pbs_common::ServiceInterface {
 public:
  PBSInstanceV3(
      std::shared_ptr<pbs_common::ConfigProviderInterface> config_provider,
      std::unique_ptr<CloudPlatformDependencyFactoryInterface>
          cloud_platform_dependency_factory);

  pbs_common::ExecutionResult Init() noexcept override;
  pbs_common::ExecutionResult Run() noexcept override;
  pbs_common::ExecutionResult Stop() noexcept override;

 private:
  pbs_common::ExecutionResult CreateComponents() noexcept;

  std::shared_ptr<pbs_common::ConfigProviderInterface> config_provider_;

  std::shared_ptr<pbs_common::AsyncExecutorInterface> async_executor_;
  std::shared_ptr<pbs_common::AsyncExecutorInterface> io_async_executor_;
  std::shared_ptr<pbs_common::HttpClientInterface> http1_client_;
  std::shared_ptr<pbs_common::HttpClientInterface> http2_client_;
  std::shared_ptr<pbs_common::AuthorizationProxyInterface> authorization_proxy_;
  std::shared_ptr<pbs_common::AuthorizationProxyInterface>
      pass_thru_authorization_proxy_;
  std::shared_ptr<pbs_common::HttpServerInterface> http_server_;
  std::shared_ptr<pbs_common::HttpServerInterface> health_http_server_;
  std::shared_ptr<pbs_common::ServiceInterface> health_service_;
  std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
      budget_consumption_helper_;
  std::shared_ptr<pbs::FrontEndServiceInterface> front_end_service_;

  PBSInstanceConfig pbs_instance_config_;

  std::unique_ptr<CloudPlatformDependencyFactoryInterface>
      cloud_platform_dependency_factory_;

  std::unique_ptr<pbs_common::MetricRouter> metric_router_;
  std::string container_type_;
};

}  // namespace privacy_sandbox::pbs

#endif  // CC_PBS_PBS_SERVER_SRC_PBS_INSTANCE_PBS_INSTANCE_V3_H_
