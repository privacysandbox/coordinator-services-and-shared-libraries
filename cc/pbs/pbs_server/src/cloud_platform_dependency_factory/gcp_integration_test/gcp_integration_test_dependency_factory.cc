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

#include "gcp_integration_test_dependency_factory.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "cc/core/authorization_proxy/mock/mock_authorization_proxy.h"
#include "cc/core/blob_storage_provider/src/gcp/gcp_cloud_storage.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/core/interface/credentials_provider_interface.h"
#include "cc/core/nosql_database_provider/src/gcp/gcp_spanner.h"
#include "cc/core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "cc/core/token_provider_cache/src/auto_refresh_token_provider.h"
#include "cc/cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_provider.h"
#include "cc/cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_provider.h"
#include "cc/pbs/authorization/src/aws/aws_http_request_response_auth_interceptor.h"
#include "cc/pbs/authorization/src/gcp/gcp_http_request_response_auth_interceptor.h"
#include "cc/pbs/authorization_token_fetcher/src/gcp/gcp_authorization_token_fetcher.h"
#include "cc/pbs/consume_budget/src/gcp/consume_budget.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/pbs_client_interface.h"
#include "cc/pbs/pbs_client/src/pbs_client.h"
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/dummy_impls.h"
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/gcp_dependency_factory.h"
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp_integration_test/test_gcp_spanner.h"
#include "cc/public/cpio/mock/metric_client/mock_metric_client.h"
#include "googlemock/include/gmock/gmock-actions.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AuthorizationProxyInterface;
using google::scp::core::AutoRefreshTokenProviderService;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::CredentialsProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::authorization_proxy::mock::MockAuthorizationProxy;
using google::scp::core::blob_storage_provider::GcpCloudStorageProvider;
using google::scp::core::common::kZeroUuid;
using google::scp::core::nosql_database_provider::GcpSpanner;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::cpio::MockMetricClient;
using google::scp::cpio::client_providers::GcpMetricClientProvider;
using google::scp::pbs::GcpAuthorizationTokenFetcher;
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
using testing::Return;

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
using ::google::scp::core::NoSQLDatabaseProviderInterface;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::authorization_proxy::mock::MockAuthorizationProxy;
using ::google::scp::core::blob_storage_provider::GcpCloudStorageProvider;
using ::google::scp::core::common::kZeroUuid;
using ::google::scp::core::nosql_database_provider::GcpSpanner;
using ::google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using ::google::scp::cpio::MockMetricClient;
using ::google::scp::cpio::client_providers::GcpMetricClientProvider;
using ::google::scp::pbs::GcpAuthorizationTokenFetcher;
using ::testing::Return;

// TODO move these to a common place
static constexpr char kBudgetKeyTablePartitionKeyName[] = "Budget_Key";
static constexpr char kBudgetKeyTableSortKeyName[] = "Timeframe";
static constexpr char kPartitionLockTablePartitionKeyName[] = "LockId";

std::unique_ptr<core::TokenProviderCacheInterface>
GcpIntegrationTestDependencyFactory::ConstructAuthorizationTokenProviderCache(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    std::shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return std::make_unique<DummyTokenProviderCache>();
}

std::unique_ptr<core::AuthorizationProxyInterface>
GcpIntegrationTestDependencyFactory::ConstructAuthorizationProxyClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::HttpClientInterface> http_client) noexcept {
  auto proxy = std::make_unique<MockAuthorizationProxy>();
  ON_CALL(*proxy, Init).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*proxy, Run).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*proxy, Stop).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*proxy, Authorize).WillByDefault([](auto& context) {
    context.response = std::make_shared<core::AuthorizationProxyResponse>();
    context.response->authorized_metadata.authorized_domain =
        std::make_shared<std::string>(
            context.request->authorization_metadata.claimed_identity);
    context.result = SuccessExecutionResult();

    context.Finish();
    return SuccessExecutionResult();
  });
  return proxy;
}

std::unique_ptr<core::AuthorizationProxyInterface>
GcpIntegrationTestDependencyFactory::ConstructAwsAuthorizationProxyClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return ConstructAuthorizationProxyClient(async_executor, http_client);
}

std::unique_ptr<NoSQLDatabaseProviderInterface>
GcpIntegrationTestDependencyFactory::ConstructNoSQLDatabaseClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  auto table_schema_map = std::make_unique<std::unordered_map<
      std::string, std::pair<std::string, std::optional<std::string>>>>();
  table_schema_map->emplace(budget_key_table_name_,
                            std::make_pair(kBudgetKeyTablePartitionKeyName,
                                           kBudgetKeyTableSortKeyName));
  table_schema_map->emplace(
      partition_lock_table_name_,
      std::make_pair(kPartitionLockTablePartitionKeyName, std::nullopt));
  return std::make_unique<TestGcpSpanner>(async_executor, io_async_executor,
                                          config_provider_,
                                          std::move(table_schema_map));
}

std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
GcpIntegrationTestDependencyFactory::ConstructBudgetConsumptionHelper(
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

std::unique_ptr<cpio::MetricClientInterface>
GcpIntegrationTestDependencyFactory::ConstructMetricClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  auto metric_client = std::make_unique<MockMetricClient>();
  ON_CALL(*metric_client, Init).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*metric_client, Run).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*metric_client, Stop).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*metric_client, PutMetrics)
      .WillByDefault(Return(SuccessExecutionResult()));
  return metric_client;
}

std::unique_ptr<core::MetricRouter>
GcpIntegrationTestDependencyFactory::ConstructMetricRouter(
    std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  // Hard-code resource attributes for testing.
  //
  // Example output similar to implemenation of:
  // google-cloud-cpp/google/cloud/opentelemetry/internal/resource_detector_impl.cc
  const opentelemetry::sdk::resource::ResourceAttributes attributes = {
      {opentelemetry::sdk::resource::SemanticConventions::kCloudProvider,
       "gcp"},
      {opentelemetry::sdk::resource::SemanticConventions::kCloudPlatform,
       "gcp_compute_engine"},
      {opentelemetry::sdk::resource::SemanticConventions::kHostType,
       "Instance"},
      {opentelemetry::sdk::resource::SemanticConventions::kHostId,
       "8852044229993849486"},
      {opentelemetry::sdk::resource::SemanticConventions::kHostName,
       "a-pbs-test"},
  };
  auto resource = opentelemetry::sdk::resource::Resource::Create(attributes);

  return GcpDependencyFactory::ConstructMetricRouter(instance_client_provider,
                                                     std::move(resource));
}

}  // namespace google::scp::pbs
