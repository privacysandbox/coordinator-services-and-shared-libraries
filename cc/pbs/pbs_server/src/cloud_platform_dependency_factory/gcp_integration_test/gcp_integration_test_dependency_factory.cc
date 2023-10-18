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
#include <unordered_map>
#include <utility>

#include "core/authorization_proxy/mock/mock_authorization_proxy.h"
#include "core/blob_storage_provider/src/gcp/gcp_cloud_storage.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/configuration_keys.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "core/token_provider_cache/src/auto_refresh_token_provider.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_provider.h"
#include "cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_provider.h"
#include "pbs/authorization/src/gcp/gcp_http_request_response_auth_interceptor.h"
#include "pbs/authorization_token_fetcher/src/gcp/gcp_authorization_token_fetcher.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/pbs_client/src/pbs_client.h"
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/dummy_impls.h"
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/gcp_dependency_factory.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

#include "test_gcp_spanner.h"

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

// TODO move these to a common place
static constexpr char kBudgetKeyTablePartitionKeyName[] = "Budget_Key";
static constexpr char kBudgetKeyTableSortKeyName[] = "Timeframe";
static constexpr char kPartitionLockTablePartitionKeyName[] = "LockId";

unique_ptr<core::TokenProviderCacheInterface>
GcpIntegrationTestDependencyFactory::ConstructAuthorizationTokenProviderCache(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return make_unique<DummyTokenProviderCache>();
}

unique_ptr<core::AuthorizationProxyInterface>
GcpIntegrationTestDependencyFactory::ConstructAuthorizationProxyClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  auto proxy = make_unique<MockAuthorizationProxy>();
  ON_CALL(*proxy, Init).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*proxy, Run).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*proxy, Stop).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*proxy, Authorize).WillByDefault([](auto& context) {
    context.response = make_shared<core::AuthorizationProxyResponse>();
    context.response->authorized_metadata.authorized_domain =
        make_shared<string>(
            context.request->authorization_metadata.claimed_identity);
    context.result = SuccessExecutionResult();

    context.Finish();
    return SuccessExecutionResult();
  });
  return proxy;
}

unique_ptr<NoSQLDatabaseProviderInterface>
GcpIntegrationTestDependencyFactory::ConstructNoSQLDatabaseClient(
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
  return make_unique<TestGcpSpanner>(async_executor, io_async_executor,
                                     config_provider_, move(table_schema_map));
}

unique_ptr<cpio::MetricClientInterface>
GcpIntegrationTestDependencyFactory::ConstructMetricClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  auto metric_client = make_unique<MockMetricClient>();
  ON_CALL(*metric_client, Init).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*metric_client, Run).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*metric_client, Stop).WillByDefault(Return(SuccessExecutionResult()));
  ON_CALL(*metric_client, PutMetrics)
      .WillByDefault(Return(SuccessExecutionResult()));
  return metric_client;
}
}  // namespace google::scp::pbs
