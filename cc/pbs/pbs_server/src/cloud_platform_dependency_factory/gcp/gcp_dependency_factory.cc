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

#include <optional>
#include <unordered_map>
#include <utility>

#include "cc/core/authorization_proxy/src/authorization_proxy.h"
#include "cc/core/blob_storage_provider/src/gcp/gcp_cloud_storage.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/nosql_database_provider/src/gcp/gcp_spanner.h"
#include "cc/core/token_provider_cache/src/auto_refresh_token_provider.h"
#include "cc/cpio/client_providers/auth_token_provider/src/gcp/gcp_auth_token_provider.h"
#include "cc/cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_provider.h"
#include "cc/cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_provider.h"
#include "cc/pbs/authorization/src/gcp/gcp_http_request_response_auth_interceptor.h"
#include "cc/pbs/authorization_token_fetcher/src/gcp/gcp_authorization_token_fetcher.h"
#include "cc/pbs/consume_budget/src/gcp/consume_budget.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/pbs_client_interface.h"
#include "cc/pbs/pbs_client/src/pbs_client.h"
#include "core/telemetry/src/authentication/gcp_token_fetcher.h"
#include "core/telemetry/src/authentication/grpc_auth_config.h"
#include "core/telemetry/src/authentication/grpc_id_token_authenticator.h"
#include "core/telemetry/src/authentication/token_fetcher.h"
#include "core/telemetry/src/common/telemetry_configuration.h"
#include "core/telemetry/src/metric/metric_router.h"
#include "core/telemetry/src/metric/otlp_grpc_authed_metric_exporter.h"
#include "google/cloud/opentelemetry/resource_detector.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/resource/resource_detector.h"

#include "dummy_impls.h"

namespace google::scp::pbs {

using ::google::scp::core::AsyncContext;
using ::google::scp::core::AsyncExecutorInterface;
using ::google::scp::core::AuthorizationProxyInterface;
using ::google::scp::core::AutoRefreshTokenProviderService;
using ::google::scp::core::BlobStorageProviderInterface;
using ::google::scp::core::ConfigProviderInterface;
using ::google::scp::core::CredentialsProviderInterface;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::GcpTokenFetcher;
using ::google::scp::core::GrpcAuthConfig;
using ::google::scp::core::GrpcIdTokenAuthenticator;
using ::google::scp::core::MetricRouter;
using ::google::scp::core::NoSQLDatabaseProviderInterface;
using ::google::scp::core::OtlpGrpcAuthedMetricExporter;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::TimeDuration;
using ::google::scp::core::blob_storage_provider::GcpCloudStorageProvider;
using ::google::scp::core::common::kZeroUuid;
using ::google::scp::core::nosql_database_provider::GcpSpanner;
using ::google::scp::cpio::MetricClientOptions;
using ::google::scp::cpio::client_providers::GcpAuthTokenProvider;
using ::google::scp::cpio::client_providers::GcpInstanceClientProvider;
using ::google::scp::cpio::client_providers::GcpMetricClientProvider;
using ::google::scp::cpio::client_providers::MetricBatchingOptions;
using ::google::scp::pbs::GcpAuthorizationTokenFetcher;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::move;
using std::nullopt;
using std::optional;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::chrono::milliseconds;

static constexpr char kGcpDependencyProvider[] = "kGCPDependencyProvider";

// TODO move these to a common place
static constexpr char kBudgetKeyTablePartitionKeyName[] = "Budget_Key";
static constexpr char kBudgetKeyTableSortKeyName[] = "Timeframe";
static constexpr char kPartitionLockTablePartitionKeyName[] = "LockId";
static constexpr TimeDuration kDefaultMetricBatchTimeDuration = 3000;

GcpDependencyFactory::GcpDependencyFactory(
    shared_ptr<ConfigProviderInterface> config_provider)
    : config_provider_(config_provider),
      metrics_batch_push_enabled_(false),
      metrics_batch_time_duration_ms_(kDefaultMetricBatchTimeDuration) {}

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
      config_provider_->Get(kServiceMetricsNamespace, metrics_namespace_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kGcpDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read metrics namespace.");
    return execution_result;
  }

  execution_result = config_provider_->Get(kServiceMetricsBatchPush,
                                           metrics_batch_push_enabled_);
  if (!execution_result.Successful()) {
    // If config of metrics_batch_push_enabled not present, continue with
    // unbatch mode.
    SCP_INFO(kGcpDependencyProvider, kZeroUuid,
             "%s flag not specified. Starting PBS in single metric push mode",
             kServiceMetricsBatchPush);
    metrics_batch_push_enabled_ = false;
  }

  execution_result = config_provider_->Get(kServiceMetricsBatchTimeDurationMs,
                                           metrics_batch_time_duration_ms_);
  if (!execution_result.Successful()) {
    SCP_INFO(
        kGcpDependencyProvider, kZeroUuid,
        "%s flag not specified. Set the time duration of batch push to 3000 "
        "milliseconds",
        kServiceMetricsBatchTimeDurationMs);
    metrics_batch_time_duration_ms_ = kDefaultMetricBatchTimeDuration;
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

  return SuccessExecutionResult();
}

unique_ptr<core::TokenProviderCacheInterface>
GcpDependencyFactory::ConstructAuthorizationTokenProviderCache(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  auto auth_token_fetcher = make_unique<GcpAuthorizationTokenFetcher>(
      http_client, remote_coordinator_auth_gateway_endpoint_, async_executor);
  return make_unique<AutoRefreshTokenProviderService>(move(auth_token_fetcher),
                                                      async_executor);
}

unique_ptr<AuthorizationProxyInterface>
GcpDependencyFactory::ConstructAuthorizationProxyClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return make_unique<core::AuthorizationProxy>(
      auth_service_endpoint_, async_executor, http_client,
      std::make_unique<GcpHttpRequestResponseAuthInterceptor>(
          config_provider_));
}

unique_ptr<BlobStorageProviderInterface>
GcpDependencyFactory::ConstructBlobStorageClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  return make_unique<GcpCloudStorageProvider>(
      async_executor, io_async_executor, config_provider_,
      async_execution_priority, io_async_execution_priority);
}

unique_ptr<NoSQLDatabaseProviderInterface>
GcpDependencyFactory::ConstructNoSQLDatabaseClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  auto table_schema_map =
      make_unique<unordered_map<string, pair<string, optional<string>>>>();
  table_schema_map->emplace(
      budget_key_table_name_,
      make_pair(kBudgetKeyTablePartitionKeyName, kBudgetKeyTableSortKeyName));
  table_schema_map->emplace(
      partition_lock_table_name_,
      make_pair(kPartitionLockTablePartitionKeyName, nullopt));
  return make_unique<GcpSpanner>(async_executor, io_async_executor,
                                 config_provider_, move(table_schema_map),
                                 async_execution_priority,
                                 io_async_execution_priority);
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

unique_ptr<cpio::MetricClientInterface>
GcpDependencyFactory::ConstructMetricClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  auto metric_client_options = make_shared<MetricClientOptions>();
  auto metric_batching_options = make_shared<MetricBatchingOptions>();
  metric_batching_options->metric_namespace = metrics_namespace_;
  metric_batching_options->enable_batch_recording = metrics_batch_push_enabled_;
  metric_batching_options->batch_recording_time_duration =
      milliseconds(metrics_batch_time_duration_ms_);
  return make_unique<GcpMetricClientProvider>(
      metric_client_options, instance_client_provider, async_executor,
      metric_batching_options);
}

unique_ptr<cpio::client_providers::AuthTokenProviderInterface>
GcpDependencyFactory::ConstructInstanceAuthorizer(
    std::shared_ptr<core::HttpClientInterface> http1_client) noexcept {
  return make_unique<GcpAuthTokenProvider>(http1_client);
}

unique_ptr<cpio::client_providers::InstanceClientProviderInterface>
GcpDependencyFactory::ConstructInstanceMetadataClient(
    shared_ptr<core::HttpClientInterface> http1_client,
    shared_ptr<core::HttpClientInterface> http2_client,
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
        auth_token_provider) noexcept {
  return make_unique<GcpInstanceClientProvider>(auth_token_provider,
                                                http1_client, http2_client);
}

unique_ptr<pbs::PrivacyBudgetServiceClientInterface>
GcpDependencyFactory::ConstructRemoteCoordinatorPBSClient(
    shared_ptr<core::HttpClientInterface> http_client,
    shared_ptr<core::TokenProviderCacheInterface>
        auth_token_provider_cache) noexcept {
  return make_unique<PrivacyBudgetServiceClient>(
      reporting_origin_for_remote_coordinator_, remote_coordinator_endpoint_,
      http_client, auth_token_provider_cache);
}

std::unique_ptr<core::MetricRouter> GcpDependencyFactory::ConstructMetricRouter(
    std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  auto resource_detector = google::cloud::otel::MakeResourceDetector();
  opentelemetry::sdk::resource::Resource resource = resource_detector->Detect();

  return this->ConstructMetricRouter(instance_client_provider,
                                     std::move(resource));
}

std::unique_ptr<core::MetricRouter> GcpDependencyFactory::ConstructMetricRouter(
    std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider,
    opentelemetry::sdk::resource::Resource resource) noexcept {
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

  opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions exporter_options;
  exporter_options.endpoint = exporter_path;

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
      metric_exporter = std::make_unique<OtlpGrpcAuthedMetricExporter>(
          exporter_options, std::move(metric_id_token_authenticator));

  return std::make_unique<MetricRouter>(config_provider_, std::move(resource),
                                        std::move(metric_exporter));
}

}  // namespace google::scp::pbs
