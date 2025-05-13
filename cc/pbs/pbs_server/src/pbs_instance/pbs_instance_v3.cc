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

#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance_v3.h"

#include <memory>
#include <string>
#include <utility>

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/authorization_proxy/src/pass_thru_authorization_proxy.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/http2_client/src/http2_client.h"
#include "cc/core/http2_server/src/http2_server.h"
#include "cc/pbs/front_end_service/src/front_end_service_v2.h"
#include "cc/pbs/health_service/src/health_service.h"
#include "cc/pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "cc/pbs/pbs_server/src/pbs_instance/error_codes.h"
#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance_configuration.h"
#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance_logging.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs {

using ::privacy_sandbox::pbs_common::AsyncExecutor;
using ::privacy_sandbox::pbs_common::AuthorizationProxyInterface;
using ::privacy_sandbox::pbs_common::ConfigProviderInterface;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::Http2Server;
using ::privacy_sandbox::pbs_common::Http2ServerOptions;
using ::privacy_sandbox::pbs_common::HttpClient;
using ::privacy_sandbox::pbs_common::HttpClientOptions;
using ::privacy_sandbox::pbs_common::kZeroUuid;
using ::privacy_sandbox::pbs_common::PassThruAuthorizationProxy;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;

PBSInstanceV3::PBSInstanceV3(
    std::shared_ptr<ConfigProviderInterface> config_provider,
    std::unique_ptr<CloudPlatformDependencyFactoryInterface>
        cloud_platform_dependency_factory)
    : config_provider_(std::move(config_provider)),
      cloud_platform_dependency_factory_(
          std::move(cloud_platform_dependency_factory)) {
  auto execution_result =
      config_provider_->Get(kContainerType, container_type_);
  if (!execution_result.Successful()) {
    container_type_ = kComputeEngine;
    SCP_INFO(kPBSInstance, kZeroUuid,
             absl::StrFormat("%s flag not specified. Defaulting to Compute "
                             "Engine startup preferences.",
                             kContainerType));
  }
}

ExecutionResult PBSInstanceV3::CreateComponents() noexcept {
  // Factory should be initialized before the other components are constructed.
  INIT_PBS_COMPONENT(cloud_platform_dependency_factory_);

  bool is_otel_enabled = false;
  auto execution_result = config_provider_->Get(kOtelEnabled, is_otel_enabled);
  if (!execution_result.Successful()) {
    SCP_INFO(
        kPBSInstance, kZeroUuid,
        absl::StrFormat(
            "%s flag not specified. Not using OpenTelemetry for observability.",
            kOtelEnabled));
  }

  if (is_otel_enabled) {
    // On initialization of metric_router_, Meter Provider would be set globally
    // for PBS. Services can access the Meter Provider using
    // opentelemetry::metrics::Provider::GetMeterProvider()
    metric_router_ =
        cloud_platform_dependency_factory_->ConstructMetricRouter();
  }

  // Construct foundational components.
  async_executor_ = std::make_shared<AsyncExecutor>(
      pbs_instance_config_.async_executor_thread_pool_size,
      pbs_instance_config_.async_executor_queue_size);
  io_async_executor_ = std::make_shared<AsyncExecutor>(
      pbs_instance_config_.io_async_executor_thread_pool_size,
      pbs_instance_config_.io_async_executor_queue_size);
  http2_client_ = std::make_shared<HttpClient>(
      async_executor_, HttpClientOptions(), metric_router_.get());

  authorization_proxy_ =
      cloud_platform_dependency_factory_->ConstructAuthorizationProxyClient(
          async_executor_, http2_client_);

  pass_thru_authorization_proxy_ =
      std::make_shared<PassThruAuthorizationProxy>();

  Http2ServerOptions http2_server_options(
      pbs_instance_config_.http2_server_use_tls,
      pbs_instance_config_.http2_server_private_key_file_path,
      pbs_instance_config_.http2_server_certificate_file_path);

  std::shared_ptr<AuthorizationProxyInterface> aws_authorization_proxy =
      cloud_platform_dependency_factory_->ConstructAwsAuthorizationProxyClient(
          async_executor_, http2_client_);
  http_server_ = std::make_shared<Http2Server>(
      *pbs_instance_config_.host_address, *pbs_instance_config_.host_port,
      pbs_instance_config_.http2server_thread_pool_size, async_executor_,
      authorization_proxy_, aws_authorization_proxy, config_provider_,
      http2_server_options, metric_router_.get());

  if (container_type_ == kComputeEngine) {
    health_http_server_ = std::make_shared<Http2Server>(
        *pbs_instance_config_.host_address, *pbs_instance_config_.health_port,
        /*thread_pool_size=*/1, async_executor_, pass_thru_authorization_proxy_,
        aws_authorization_proxy, config_provider_, http2_server_options);
    health_service_ = std::make_shared<HealthService>(
        health_http_server_, config_provider_, async_executor_);
  }

  budget_consumption_helper_ =
      cloud_platform_dependency_factory_->ConstructBudgetConsumptionHelper(
          async_executor_.get(), io_async_executor_.get());
  if (budget_consumption_helper_ == nullptr) {
    SCP_WARNING(kPBSInstance, kZeroUuid,
                "BudgetConsumptionHelper is unavailable.");
    return FailureExecutionResult(SC_PBS_SERVICE_INITIALIZATION_ERROR);
  }

  front_end_service_ = std::make_shared<FrontEndServiceV2>(
      http_server_, async_executor_, config_provider_,
      budget_consumption_helper_.get(), metric_router_.get());

  return SuccessExecutionResult();
}

ExecutionResult PBSInstanceV3::Init() noexcept {
  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceV3 attempting to initialize.");

  ASSIGN_OR_RETURN(pbs_instance_config_,
                   GetPBSInstanceConfigFromConfigProvider(config_provider_));

  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceV3 constructing dependencies.");
  RETURN_IF_FAILURE(CreateComponents());

  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceV3 initializing dependencies.");
  INIT_PBS_COMPONENT(async_executor_);
  INIT_PBS_COMPONENT(io_async_executor_);
  INIT_PBS_COMPONENT(http2_client_);
  INIT_PBS_COMPONENT(authorization_proxy_);
  INIT_PBS_COMPONENT(pass_thru_authorization_proxy_);
  INIT_PBS_COMPONENT(http_server_);
  INIT_PBS_COMPONENT(budget_consumption_helper_);
  INIT_PBS_COMPONENT(front_end_service_);

  if (container_type_ == kComputeEngine) {
    INIT_PBS_COMPONENT(health_http_server_);
    INIT_PBS_COMPONENT(health_service_);
  }

  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceV3 has been initialized.");

  return SuccessExecutionResult();
}

ExecutionResult PBSInstanceV3::Run() noexcept {
  SCP_INFO(kPBSInstance, kZeroUuid,
           "PBSInstanceV3 attempting to run components.");

  RUN_PBS_COMPONENT(async_executor_);
  RUN_PBS_COMPONENT(io_async_executor_);
  RUN_PBS_COMPONENT(http2_client_);
  RUN_PBS_COMPONENT(authorization_proxy_);
  RUN_PBS_COMPONENT(pass_thru_authorization_proxy_);
  RUN_PBS_COMPONENT(http_server_);
  RUN_PBS_COMPONENT(budget_consumption_helper_);
  RUN_PBS_COMPONENT(front_end_service_);

  if (container_type_ == kComputeEngine) {
    RUN_PBS_COMPONENT(health_http_server_);
    RUN_PBS_COMPONENT(health_service_);
  }

  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceV3 components have been run.");

  return SuccessExecutionResult();
}

ExecutionResult PBSInstanceV3::Stop() noexcept {
  SCP_INFO(kPBSInstance, kZeroUuid,
           "PBSInstanceV3 attempting to stop components.");

  STOP_PBS_COMPONENT(front_end_service_);
  STOP_PBS_COMPONENT(budget_consumption_helper_);
  STOP_PBS_COMPONENT(health_service_);
  STOP_PBS_COMPONENT(pass_thru_authorization_proxy_);
  STOP_PBS_COMPONENT(authorization_proxy_);
  STOP_PBS_COMPONENT(http2_client_);
  STOP_PBS_COMPONENT(io_async_executor_);
  STOP_PBS_COMPONENT(async_executor_);

  if (container_type_ == kComputeEngine) {
    STOP_PBS_COMPONENT(health_http_server_);
    STOP_PBS_COMPONENT(http_server_);
  }

  SCP_INFO(kPBSInstance, kZeroUuid, "PBSInstanceV3 components have stopped.");

  return SuccessExecutionResult();
}

}  // namespace privacy_sandbox::pbs
