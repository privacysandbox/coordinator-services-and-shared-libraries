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

#include <string>
#include <utility>
#include <vector>

#include "cc/pbs/consume_budget/src/gcp/consume_budget.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/configuration_keys.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/telemetry/mock/in_memory_metric_exporter.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/resource/resource_detector.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/pbs_client/src/pbs_client.h"

#include "local_authorization_proxy.h"
#include "local_instance_metadata_client.h"
#include "local_metric_client.h"
#include "local_token_provider_cache.h"

namespace google::scp::pbs {

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AuthorizationProxyInterface;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::InMemoryMetricExporter;
using google::scp::core::MetricRouter;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;

static constexpr char kLocalDependencyProvider[] = "kLocalDependencyProvider";

ExecutionResult LocalDependencyFactory::Init() noexcept {
  SCP_INFO(kLocalDependencyProvider, kZeroUuid,
           "Initializing Local dependency factory");

  RETURN_IF_FAILURE(ReadConfigurations());
  return SuccessExecutionResult();
}

ExecutionResult LocalDependencyFactory::ReadConfigurations() {
  // Ignore key not being present.
  config_provider_->Get(kRemotePrivacyBudgetServiceHostAddress,
                        remote_coordinator_endpoint_);
  return SuccessExecutionResult();
}

std::unique_ptr<core::TokenProviderCacheInterface>
LocalDependencyFactory::ConstructAuthorizationTokenProviderCache(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    std::shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return std::make_unique<LocalTokenProviderCache>();
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

std::unique_ptr<BlobStorageProviderInterface>
LocalDependencyFactory::ConstructBlobStorageClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  return std::make_unique<MockBlobStorageProvider>();
}

void InitializeInMemoryDatabase(
    std::unique_ptr<MockNoSQLDatabaseProvider>& nosql_database_provider,
    const std::vector<core::PartitionId>& partition_ids) {
  nosql_database_provider->InitializeTable("budget", "Budget_Key", "Timeframe");
  std::vector<std::string> partition_ids_strings;
  for (auto& partition_id : partition_ids) {
    partition_ids_strings.push_back(ToString(partition_id));
  }
  // Initialize one row each for the partitions
  nosql_database_provider->InitializeTableWithDefaultRows(
      "partition_lock_table", "LockId", partition_ids_strings);
}

std::unique_ptr<NoSQLDatabaseProviderInterface>
LocalDependencyFactory::ConstructNoSQLDatabaseClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  // Share the in-memory database for all the nosql database clients. This
  // allows multiple PBS instances to be run in a test case.
  static auto in_memory_database =
      std::make_shared<MockNoSQLDatabaseProvider::InMemoryDatabase>();
  auto nosql_database_provider =
      std::make_unique<MockNoSQLDatabaseProvider>(in_memory_database);
  InitializeInMemoryDatabase(nosql_database_provider, partition_ids_);
  return std::move(nosql_database_provider);
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

std::unique_ptr<cpio::MetricClientInterface>
LocalDependencyFactory::ConstructMetricClient(
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  return std::make_unique<LocalMetricClient>();
}

std::unique_ptr<cpio::client_providers::InstanceClientProviderInterface>
LocalDependencyFactory::ConstructInstanceMetadataClient(
    std::shared_ptr<core::HttpClientInterface> http1_client,
    std::shared_ptr<core::HttpClientInterface> http2_client,
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    std::shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
        auth_token_provider) noexcept {
  return std::make_unique<LocalInstanceClientProvider>();
}

std::unique_ptr<cpio::client_providers::AuthTokenProviderInterface>
LocalDependencyFactory::ConstructInstanceAuthorizer(
    std::shared_ptr<core::HttpClientInterface> http1_client) noexcept {
  return nullptr;
}

std::unique_ptr<pbs::PrivacyBudgetServiceClientInterface>
LocalDependencyFactory::ConstructRemoteCoordinatorPBSClient(
    std::shared_ptr<core::HttpClientInterface> http_client,
    std::shared_ptr<core::TokenProviderCacheInterface>
        auth_token_provider_cache) noexcept {
  return std::make_unique<PrivacyBudgetServiceClient>(
      reporting_origin_, remote_coordinator_endpoint_, http_client,
      auth_token_provider_cache);
}

std::unique_ptr<core::MetricRouter>
LocalDependencyFactory::ConstructMetricRouter(
    std::shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
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
  opentelemetry::sdk::resource::Resource resource = resource_detector.Detect();

  return std::make_unique<MetricRouter>(config_provider_, std::move(resource),
                                        std::move(metric_exporter));
}

}  // namespace google::scp::pbs
