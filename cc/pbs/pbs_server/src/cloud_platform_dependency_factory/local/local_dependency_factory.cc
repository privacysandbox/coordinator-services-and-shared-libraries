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

#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/configuration_keys.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/pbs_client/src/pbs_client.h"

#include "local_authorization_proxy.h"
#include "local_instance_metadata_client.h"
#include "local_metric_client.h"
#include "local_token_provider_cache.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AuthorizationProxyInterface;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::CredentialsProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

namespace google::scp::pbs {

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

unique_ptr<core::TokenProviderCacheInterface>
LocalDependencyFactory::ConstructAuthorizationTokenProviderCache(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return make_unique<LocalTokenProviderCache>();
}

unique_ptr<AuthorizationProxyInterface>
LocalDependencyFactory::ConstructAuthorizationProxyClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return make_unique<LocalAuthorizationProxy>();
}

unique_ptr<BlobStorageProviderInterface>
LocalDependencyFactory::ConstructBlobStorageClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  return make_unique<MockBlobStorageProvider>();
}

void InitializeInMemoryDatabase(
    unique_ptr<MockNoSQLDatabaseProvider>& nosql_database_provider,
    const vector<core::PartitionId>& partition_ids) {
  nosql_database_provider->InitializeTable("budget", "Budget_Key", "Timeframe");
  vector<std::string> partition_ids_strings;
  for (auto& partition_id : partition_ids) {
    partition_ids_strings.push_back(ToString(partition_id));
  }
  // Initialize one row each for the partitions
  nosql_database_provider->InitializeTableWithDefaultRows(
      "partition_lock_table", "LockId", partition_ids_strings);
}

unique_ptr<NoSQLDatabaseProviderInterface>
LocalDependencyFactory::ConstructNoSQLDatabaseClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  // Share the in-memory database for all the nosql database clients. This
  // allows multiple PBS instances to be run in a test case.
  static auto in_memory_database =
      make_shared<MockNoSQLDatabaseProvider::InMemoryDatabase>();
  auto nosql_database_provider =
      make_unique<MockNoSQLDatabaseProvider>(in_memory_database);
  InitializeInMemoryDatabase(nosql_database_provider, partition_ids_);
  return move(nosql_database_provider);
}

unique_ptr<cpio::MetricClientInterface>
LocalDependencyFactory::ConstructMetricClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  return make_unique<LocalMetricClient>();
}

unique_ptr<cpio::client_providers::InstanceClientProviderInterface>
LocalDependencyFactory::ConstructInstanceMetadataClient(
    shared_ptr<core::HttpClientInterface> http1_client,
    shared_ptr<core::HttpClientInterface> http2_client,
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
        auth_token_provider) noexcept {
  return make_unique<LocalInstanceClientProvider>();
}

unique_ptr<cpio::client_providers::AuthTokenProviderInterface>
LocalDependencyFactory::ConstructInstanceAuthorizer(
    shared_ptr<core::HttpClientInterface> http1_client) noexcept {
  return nullptr;
}

unique_ptr<pbs::PrivacyBudgetServiceClientInterface>
LocalDependencyFactory::ConstructRemoteCoordinatorPBSClient(
    shared_ptr<core::HttpClientInterface> http_client,
    shared_ptr<core::TokenProviderCacheInterface>
        auth_token_provider_cache) noexcept {
  return make_unique<PrivacyBudgetServiceClient>(
      reporting_origin_, remote_coordinator_endpoint_, http_client,
      auth_token_provider_cache);
}

}  // namespace google::scp::pbs
