// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "local_dependency_factory.h"

#include <utility>

#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/core/telemetry/mock/in_memory_metric_exporter.h"
#include "cc/pbs/consume_budget/src/gcp/consume_budget.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_authorization_proxy.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/resource_detector.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"

namespace google::scp::pbs {

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AuthorizationProxyInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::InMemoryMetricExporter;
using google::scp::core::MetricRouter;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;

static constexpr char kLocalDependencyProvider[] = "kLocalDependencyProvider";

ExecutionResult LocalDependencyFactory::Init() noexcept {
  SCP_INFO(kLocalDependencyProvider, kZeroUuid,
           "Initializing Local dependency factory");

  RETURN_IF_FAILURE(ReadConfigurations());
  return SuccessExecutionResult();
}

ExecutionResult LocalDependencyFactory::ReadConfigurations() {
  return SuccessExecutionResult();
}

std::unique_ptr<AuthorizationProxyInterface>
LocalDependencyFactory::ConstructAuthorizationProxyClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return std::make_unique<LocalAuthorizationProxy>();
}

std::unique_ptr<AuthorizationProxyInterface>
LocalDependencyFactory::ConstructAwsAuthorizationProxyClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return nullptr;
}

std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
LocalDependencyFactory::ConstructBudgetConsumptionHelper(
    core::AsyncExecutorInterface* async_executor,
    core::AsyncExecutorInterface* io_async_executor) noexcept {
  google::scp::core::ExecutionResultOr<
      std::shared_ptr<cloud::spanner::Connection>>
      spanner_connection =
          BudgetConsumptionHelper::MakeSpannerConnectionForProd(
              *config_provider_);
  if (!spanner_connection.result().Successful()) {
    return nullptr;
  }
  return std::make_unique<pbs::BudgetConsumptionHelper>(
      config_provider_.get(), async_executor, io_async_executor,
      std::move(*spanner_connection));
}

std::unique_ptr<core::MetricRouter>
LocalDependencyFactory::ConstructMetricRouter() noexcept {
  bool is_otel_print_data_to_console_enabled = false;
  config_provider_->Get(kOtelPrintDataToConsoleEnabled,
                        is_otel_print_data_to_console_enabled);

  // We would not be using any token fetching (no authentication) for local.
  // Instead, we are using in_memory_metric_exporter to store the data locally
  // in memory
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
      metric_exporter = std::make_unique<InMemoryMetricExporter>(
          is_otel_print_data_to_console_enabled);

  opentelemetry::sdk::resource::OTELResourceDetector resource_detector;
  opentelemetry::sdk::resource::ResourceAttributes resource_attributes = {
      {opentelemetry::sdk::resource::SemanticConventions::kServiceName, "pbs"},
  };

  opentelemetry::sdk::resource::Resource resource = resource_detector.Detect();
  resource = resource.Merge(
      opentelemetry::sdk::resource::Resource::Create(resource_attributes));

  return std::make_unique<MetricRouter>(config_provider_, std::move(resource),
                                        std::move(metric_exporter));
}

}  // namespace google::scp::pbs
