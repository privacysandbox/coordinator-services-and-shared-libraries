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

#include "gcp_dependency_factory.h"

#include <unordered_map>
#include <utility>
#include <variant>

#include "cc/core/authorization_proxy/src/authorization_proxy.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/core/telemetry/src/authentication/gcp_token_fetcher.h"
#include "cc/core/telemetry/src/authentication/grpc_auth_config.h"
#include "cc/core/telemetry/src/authentication/grpc_id_token_authenticator.h"
#include "cc/core/telemetry/src/authentication/token_fetcher.h"
#include "cc/core/telemetry/src/common/telemetry_configuration.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/core/telemetry/src/metric/otlp_grpc_authed_metric_exporter.h"
#include "cc/pbs/authorization/src/gcp/gcp_http_request_response_auth_interceptor.h"
#include "cc/pbs/consume_budget/src/gcp/consume_budget.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "google/cloud/monitoring/v3/metric_client.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/opentelemetry/resource_detector.h"
#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/resource/resource_detector.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"

namespace google::scp::pbs {

using ::google::scp::core::AsyncExecutorInterface;
using ::google::scp::core::AuthorizationProxyInterface;
using ::google::scp::core::ConfigProviderInterface;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::GcpTokenFetcher;
using ::google::scp::core::GrpcAuthConfig;
using ::google::scp::core::GrpcIdTokenAuthenticator;
using ::google::scp::core::kAlternateCloudServiceRegion;
using ::google::scp::core::kHttpServerDnsRoutingEnabled;
using ::google::scp::core::MetricRouter;
using ::google::scp::core::OtlpGrpcAuthedMetricExporter;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::TimeDuration;
using ::google::scp::core::common::kZeroUuid;

static constexpr char kGcpDependencyProvider[] = "kGCPDependencyProvider";

GcpDependencyFactory::GcpDependencyFactory(
    std::shared_ptr<ConfigProviderInterface> config_provider)
    : config_provider_(config_provider) {}

ExecutionResult GcpDependencyFactory::Init() noexcept {
  RETURN_IF_FAILURE(ReadConfigurations());
  return SuccessExecutionResult();
}

ExecutionResult GcpDependencyFactory::ReadConfigurations() {
  auto execution_result =
      config_provider_->Get(kBudgetKeyTableName, budget_key_table_name_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpDependencyProvider, kZeroUuid, execution_result,
              "Failed to read budget key table name.");
    return execution_result;
  }
  execution_result = config_provider_->Get(kPBSPartitionLockTableNameConfigName,
                                           partition_lock_table_name_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpDependencyProvider, kZeroUuid, execution_result,
              "Failed to read partition key table name.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(kAuthServiceEndpoint, auth_service_endpoint_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kGcpDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read auth service endpoint.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(kRemotePrivacyBudgetServiceClaimedIdentity,
                            reporting_origin_for_remote_coordinator_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kGcpDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read remote claimed identity.");
    return execution_result;
  }

  execution_result = config_provider_->Get(
      kRemotePrivacyBudgetServiceHostAddress, remote_coordinator_endpoint_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kGcpDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read remote host address.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(kRemotePrivacyBudgetServiceAuthServiceEndpoint,
                            remote_coordinator_auth_gateway_endpoint_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kGcpDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read remote auth endpoint.");
    return execution_result;
  }

  bool dns_routing_enabled = false;
  if (config_provider_ != nullptr) {
    execution_result = config_provider_->Get(kHttpServerDnsRoutingEnabled,
                                             dns_routing_enabled);
    if (!execution_result.Successful()) {
      dns_routing_enabled = false;
    }
  }

  execution_result = config_provider_->Get(kAlternateAuthServiceEndpoint,
                                           alternate_auth_service_endpoint_);
  if (!execution_result.Successful()) {
    if (dns_routing_enabled) {
      SCP_CRITICAL(kGcpDependencyProvider, kZeroUuid, execution_result,
                   "Failed to read AWS auth service endpoint.");
      return execution_result;
    } else {
      SCP_INFO(kGcpDependencyProvider, kZeroUuid,
               "Failed to read AWS auth service endpoint.");
    }
  }

  execution_result = config_provider_->Get(kAlternateCloudServiceRegion,
                                           alternate_cloud_service_region_);
  if (!execution_result.Successful()) {
    if (dns_routing_enabled) {
      SCP_CRITICAL(kGcpDependencyProvider, kZeroUuid, execution_result,
                   "Failed to read cloud service region.");
      return execution_result;
    } else {
      SCP_INFO(kGcpDependencyProvider, kZeroUuid,
               "Failed to read cloud service region.");
    }
  }

  return SuccessExecutionResult();
}

std::unique_ptr<AuthorizationProxyInterface>
GcpDependencyFactory::ConstructAuthorizationProxyClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return std::make_unique<core::AuthorizationProxy>(
      auth_service_endpoint_, async_executor, http_client,
      std::make_unique<GcpHttpRequestResponseAuthInterceptor>(
          config_provider_));
}

std::unique_ptr<AuthorizationProxyInterface>
GcpDependencyFactory::ConstructAwsAuthorizationProxyClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return std::make_unique<core::AuthorizationProxy>(
      alternate_auth_service_endpoint_, async_executor, http_client,
      std::make_unique<AwsHttpRequestResponseAuthInterceptor>(
          alternate_cloud_service_region_, config_provider_));
}

std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
GcpDependencyFactory::ConstructBudgetConsumptionHelper(
    google::scp::core::AsyncExecutorInterface* async_executor,
    google::scp::core::AsyncExecutorInterface* io_async_executor) noexcept {
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
GcpDependencyFactory::ConstructMetricRouter() noexcept {
  auto resource_detector = google::cloud::otel::MakeResourceDetector();
  opentelemetry::sdk::resource::ResourceAttributes resource_attributes = {
      {opentelemetry::sdk::resource::SemanticConventions::kServiceName, "pbs"},
  };

  opentelemetry::sdk::resource::Resource resource = resource_detector->Detect();
  resource = resource.Merge(
      opentelemetry::sdk::resource::Resource::Create(resource_attributes));

  return this->ConstructMetricRouter(std::move(resource));
}

std::unique_ptr<core::MetricRouter> GcpDependencyFactory::ConstructMetricRouter(
    opentelemetry::sdk::resource::Resource resource) noexcept {
  const std::string exporter_config = GetConfigValue(
      std::string(core::kOtelMetricsExporterKey),
      std::string(core::kOtelMetricsExporterValue), *config_provider_);

  if (exporter_config == "otlp") {
    SCP_INFO(kGcpDependencyProvider, kZeroUuid,
             "Using value: " + exporter_config +
                 " for option OTEL_METRICS_EXPORTER.");
    std::unique_ptr<GrpcAuthConfig> metric_auth_config =
        std::make_unique<GrpcAuthConfig>(
            GetConfigValue(std::string(core::kOtelServiceAccountKey),
                           std::string(core::kOtelServiceAccountValue),
                           *config_provider_),
            GetConfigValue(std::string(core::kOtelAudienceKey),
                           std::string(core::kOtelAudienceValue),
                           *config_provider_),
            GetConfigValue(std::string(core::kOtelCredConfigKey),
                           std::string(core::kOtelCredConfigValue),
                           *config_provider_));
    std::unique_ptr<core::TokenFetcher> metric_token_fetcher =
        std::make_unique<GcpTokenFetcher>();
    std::unique_ptr<GrpcIdTokenAuthenticator> metric_id_token_authenticator =
        std::make_unique<GrpcIdTokenAuthenticator>(
            std::move(metric_auth_config), std::move(metric_token_fetcher));

    const std::string exporter_path = GetConfigValue(
        std::string(core::kOtelExporterOtlpEndpointKey),
        std::string(core::kOtelExporterOtlpEndpointValue), *config_provider_);

    opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions
        exporter_options;
    exporter_options.endpoint = exporter_path;

    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter =
        std::make_unique<OtlpGrpcAuthedMetricExporter>(
            exporter_options, std::move(metric_id_token_authenticator));

    return std::make_unique<MetricRouter>(config_provider_, std::move(resource),
                                          std::move(exporter));
  } else if (exporter_config == "googlecloud") {
    SCP_INFO(kGcpDependencyProvider, kZeroUuid,
             "Using value: " + exporter_config +
                 " for option OTEL_METRICS_EXPORTER.");
    if (resource.GetAttributes().find(
            opentelemetry::sdk::resource::SemanticConventions::
                kCloudAccountId) == resource.GetAttributes().end()) {
      SCP_WARNING(kGcpDependencyProvider, kZeroUuid,
                  "Failed to read current project ID using "
                  "GcpResourceDetector: cannot find project ID.");
      return nullptr;
    }

    auto project_id = resource.GetAttributes().at(
        opentelemetry::sdk::resource::SemanticConventions::kCloudAccountId);
    if (!std::holds_alternative<std::string>(project_id)) {
      SCP_WARNING(kGcpDependencyProvider, kZeroUuid,
                  "Failed to read current project ID using "
                  "GcpResourceDetector: project ID is not a string.");
      return nullptr;
    }

    auto project = google::cloud::Project(std::get<std::string>(project_id));
    auto connection =
        google::cloud::monitoring_v3::MakeMetricServiceConnection();
    auto client = google::cloud::monitoring_v3::MetricServiceClient(connection);
    auto options = google::cloud::Options{}
                       .set<google::cloud::otel::MetricNameFormatterOption>(
                           [](std::string const& s) {
                             return "custom.googleapis.com/" + s;
                           });

    auto exporter = google::cloud::otel::MakeMonitoringExporter(
        project, std::move(connection), options);

    return std::make_unique<MetricRouter>(config_provider_, std::move(resource),
                                          std::move(exporter));
  }

  SCP_WARNING(kGcpDependencyProvider, kZeroUuid,
              "Invalid config value: " + exporter_config +
                  " for option OTEL_METRICS_EXPORTER.");
  return nullptr;
}

}  // namespace google::scp::pbs
