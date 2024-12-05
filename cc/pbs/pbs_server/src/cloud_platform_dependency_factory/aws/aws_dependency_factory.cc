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

#include "aws_dependency_factory.h"

#include <memory>
#include <utility>

#include <aws/core/Aws.h>

#include "cc/core/authorization_proxy/src/authorization_proxy.h"
#include "cc/core/blob_storage_provider/src/aws/aws_s3.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/credentials_provider/src/aws_assume_role_credentials_provider.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/core/interface/token_fetcher_interface.h"
#include "cc/core/nosql_database_provider/src/aws/aws_dynamo_db.h"
#include "cc/core/telemetry/src/authentication/aws_token_fetcher.h"
#include "cc/core/telemetry/src/authentication/grpc_auth_config.h"
#include "cc/core/telemetry/src/authentication/grpc_id_token_authenticator.h"
#include "cc/core/telemetry/src/authentication/token_fetcher.h"
#include "cc/core/telemetry/src/common/telemetry_configuration.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/core/telemetry/src/metric/otlp_grpc_authed_metric_exporter.h"
#include "cc/core/token_provider_cache/src/auto_refresh_token_provider.h"
#include "cc/cpio/client_providers/auth_token_provider/src/aws/aws_auth_token_provider.h"
#include "cc/cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_provider.h"
#include "cc/cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_utils.h"
#include "cc/cpio/client_providers/metric_client_provider/src/aws/aws_metric_client_provider.h"
#include "cc/pbs/authorization/src/aws/aws_http_request_response_auth_interceptor.h"
#include "cc/pbs/authorization_token_fetcher/src/aws/aws_authorization_token_fetcher.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/pbs_client_interface.h"
#include "cc/pbs/pbs_client/src/pbs_client.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AuthorizationProxyInterface;
using google::scp::core::AutoRefreshTokenProviderService;
using google::scp::core::AwsAssumeRoleCredentialsProvider;
using google::scp::core::AwsTokenFetcher;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::GrpcAuthConfig;
using google::scp::core::GrpcIdTokenAuthenticator;
using google::scp::core::MetricRouter;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::OtlpGrpcAuthedMetricExporter;
using google::scp::core::TokenFetcher;
using google::scp::core::TokenFetcherInterface;
using google::scp::core::blob_storage_provider::AwsS3Provider;
using google::scp::core::common::kZeroUuid;
using google::scp::core::nosql_database_provider::AwsDynamoDB;
using google::scp::cpio::MetricClientOptions;
using google::scp::cpio::client_providers::AwsAuthTokenProvider;
using google::scp::cpio::client_providers::AwsInstanceClientProvider;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using google::scp::cpio::client_providers::AwsMetricClientProvider;
using google::scp::cpio::client_providers::MetricBatchingOptions;
using google::scp::pbs::AwsAuthorizationTokenFetcher;
using google::scp::pbs::AwsHttpRequestResponseAuthInterceptor;
using std::make_shared;
using std::move;
using std::string;

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

namespace google::scp::pbs {

static constexpr char kAwsDependencyProvider[] = "kAWSDependencyProvider";

ExecutionResult AwsDependencyFactory::Init() noexcept {
  RETURN_IF_FAILURE(ReadConfigurations());

  SDKOptions aws_options;
  InitAPI(aws_options);

  return SuccessExecutionResult();
}

ExecutionResult AwsDependencyFactory::ReadConfigurations() {
  auto execution_result = config_provider_->Get(
      kRemotePrivacyBudgetServiceAssumeRoleArn, remote_assume_role_arn_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsDependencyProvider, kZeroUuid, execution_result,
              "Failed to read remote assume role name.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(kRemotePrivacyBudgetServiceAssumeRoleExternalId,
                            remote_assume_role_external_id_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsDependencyProvider, kZeroUuid, execution_result,
              "Failed to read the assume role external id.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(core::kCloudServiceRegion, cloud_service_region_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kAwsDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read  cloud service region.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(kAuthServiceEndpoint, auth_service_endpoint_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kAwsDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read auth service endpoint.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(kServiceMetricsNamespace, metrics_namespace_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kAwsDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read metrics namespace.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(kRemotePrivacyBudgetServiceCloudServiceRegion,
                            remote_coordinator_region_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kAwsDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read remote cloud service region.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(kRemotePrivacyBudgetServiceAuthServiceEndpoint,
                            remote_coordinator_auth_gateway_endpoint_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kAwsDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read remote auth endpoint.");
    return execution_result;
  }

  execution_result =
      config_provider_->Get(kRemotePrivacyBudgetServiceClaimedIdentity,
                            reporting_origin_for_remote_coordinator_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kAwsDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read remote claimed identity.");
    return execution_result;
  }

  execution_result = config_provider_->Get(
      kRemotePrivacyBudgetServiceHostAddress, remote_coordinator_endpoint_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kAwsDependencyProvider, kZeroUuid, execution_result,
                 "Failed to read remote host address.");
    return execution_result;
  }

  // Optional
  execution_result = config_provider_->Get(kServiceMetricsBatchPush,
                                           metrics_batch_push_enabled_);
  if (!execution_result.Successful()) {
    // If config of metrics_batch_push_enabled not present, continue with
    // unbatch mode.
    SCP_INFO(kAwsDependencyProvider, kZeroUuid,
             "%s flag not specified. Starting PBS in single metric push mode",
             kServiceMetricsBatchPush);
    metrics_batch_push_enabled_ = false;
  }

  return SuccessExecutionResult();
}

unique_ptr<core::TokenProviderCacheInterface>
AwsDependencyFactory::ConstructAuthorizationTokenProviderCache(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  auto credentials_provider = make_unique<AwsAssumeRoleCredentialsProvider>(
      make_shared<string>(remote_assume_role_arn_),
      make_shared<string>(remote_assume_role_external_id_), async_executor,
      io_async_executor, make_shared<string>(cloud_service_region_));
  auto auth_token_fetcher = make_unique<AwsAuthorizationTokenFetcher>(
      remote_coordinator_auth_gateway_endpoint_, cloud_service_region_,
      move(credentials_provider));
  return make_unique<AutoRefreshTokenProviderService>(move(auth_token_fetcher),
                                                      async_executor);
}

unique_ptr<AuthorizationProxyInterface>
AwsDependencyFactory::ConstructAuthorizationProxyClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return make_unique<core::AuthorizationProxy>(
      auth_service_endpoint_, async_executor, http_client,
      std::make_unique<AwsHttpRequestResponseAuthInterceptor>(
          cloud_service_region_, config_provider_));
}

unique_ptr<AuthorizationProxyInterface>
AwsDependencyFactory::ConstructAwsAuthorizationProxyClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return nullptr;
}

unique_ptr<BlobStorageProviderInterface>
AwsDependencyFactory::ConstructBlobStorageClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  return make_unique<AwsS3Provider>(async_executor, io_async_executor,
                                    config_provider_);
}

unique_ptr<NoSQLDatabaseProviderInterface>
AwsDependencyFactory::ConstructNoSQLDatabaseClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  return make_unique<AwsDynamoDB>(async_executor, io_async_executor,
                                  config_provider_);
}

std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
AwsDependencyFactory::ConstructBudgetConsumptionHelper(
    core::AsyncExecutorInterface* async_executor,
    core::AsyncExecutorInterface* io_async_executor) noexcept {
  return nullptr;
}

unique_ptr<cpio::MetricClientInterface>
AwsDependencyFactory::ConstructMetricClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  auto metric_client_options = make_shared<MetricClientOptions>();
  auto metric_batching_options = make_shared<MetricBatchingOptions>();
  metric_batching_options->metric_namespace = metrics_namespace_;
  metric_batching_options->enable_batch_recording = metrics_batch_push_enabled_;

  return make_unique<AwsMetricClientProvider>(
      metric_client_options, instance_client_provider, async_executor,
      io_async_executor, metric_batching_options);
}

unique_ptr<cpio::client_providers::AuthTokenProviderInterface>
AwsDependencyFactory::ConstructInstanceAuthorizer(
    std::shared_ptr<core::HttpClientInterface> http1_client) noexcept {
  return make_unique<AwsAuthTokenProvider>(http1_client);
}

unique_ptr<cpio::client_providers::InstanceClientProviderInterface>
AwsDependencyFactory::ConstructInstanceMetadataClient(
    std::shared_ptr<core::HttpClientInterface> http1_client,
    std::shared_ptr<core::HttpClientInterface> http2_client,
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    std::shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
        auth_token_provider) noexcept {
  return make_unique<AwsInstanceClientProvider>(
      auth_token_provider, http1_client, async_executor, io_async_executor);
}

unique_ptr<pbs::PrivacyBudgetServiceClientInterface>
AwsDependencyFactory::ConstructRemoteCoordinatorPBSClient(
    shared_ptr<core::HttpClientInterface> http_client,
    shared_ptr<core::TokenProviderCacheInterface>
        auth_token_provider_cache) noexcept {
  return make_unique<PrivacyBudgetServiceClient>(
      reporting_origin_for_remote_coordinator_, remote_coordinator_endpoint_,
      http_client, auth_token_provider_cache);
}

std::unique_ptr<core::MetricRouter> AwsDependencyFactory::ConstructMetricRouter(
    absl::Nullable<std::shared_ptr<
        cpio::client_providers::InstanceClientProviderInterface>>
        instance_client_provider) noexcept {
  std::string current_instance_resource_name;
  CHECK(instance_client_provider != nullptr);
  ExecutionResult execution_result =
      instance_client_provider->GetCurrentInstanceResourceNameSync(
          current_instance_resource_name);

  opentelemetry::sdk::resource::ResourceAttributes resource_attributes = {
      {opentelemetry::sdk::resource::SemanticConventions::kCloudProvider,
       "aws"},
      {opentelemetry::sdk::resource::SemanticConventions::kCloudPlatform,
       "aws_ec2"},
      {opentelemetry::sdk::resource::SemanticConventions::kServiceName, "pbs"},
  };
  if (execution_result.Successful()) {
    auto cloud_region_or = AwsInstanceClientUtils::ParseRegionFromResourceName(
        current_instance_resource_name);
    auto cloud_account_id_or =
        AwsInstanceClientUtils::ParseAccountIdFromResourceName(
            current_instance_resource_name);
    auto host_id_or =
        AwsInstanceClientUtils::ParseInstanceIdFromInstanceResourceName(
            current_instance_resource_name);

    if (cloud_region_or.result().Successful()) {
      // This is the region string formatted for Google Cloud monitoring.
      //
      // The format of this field is "aws:{region}", where supported values for
      // {region} are listed at
      // http://docs.aws.amazon.com/general/latest/gr/rande.html.
      // https://cloud.google.com/monitoring/api/resources#tag_aws_ec2_instance
      std::string cloud_region = "aws:" + *cloud_region_or;
      resource_attributes.insert(
          {opentelemetry::sdk::resource::SemanticConventions::kCloudRegion,
           cloud_region});
    }
    if (cloud_account_id_or.result().Successful()) {
      resource_attributes.insert(
          {opentelemetry::sdk::resource::SemanticConventions::kCloudAccountId,
           move(*cloud_account_id_or)});
    }
    if (host_id_or.result().Successful()) {
      resource_attributes.insert(
          {opentelemetry::sdk::resource::SemanticConventions::kHostId,
           move(*host_id_or)});
    }
  } else {
    SCP_ERROR(kAwsDependencyProvider, kZeroUuid, execution_result,
              "Failed to retrieve AWS Resource attributes.");
  }
  auto resource =
      opentelemetry::sdk::resource::Resource::Create(resource_attributes);

  return this->ConstructMetricRouter(instance_client_provider,
                                     std::move(resource));
}

std::unique_ptr<core::MetricRouter> AwsDependencyFactory::ConstructMetricRouter(
    absl::Nullable<std::shared_ptr<
        cpio::client_providers::InstanceClientProviderInterface>>
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
  std::unique_ptr<TokenFetcher> metric_token_fetcher =
      std::make_unique<AwsTokenFetcher>();
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
