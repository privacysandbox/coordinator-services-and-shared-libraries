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

#include "aws_integration_test_dependency_factory.h"

#include <memory>

#include "core/authorization_proxy/mock/mock_authorization_proxy.h"
#include "core/common/uuid/src/uuid.h"
#include "core/credentials_provider/mock/mock_aws_credentials_provider.h"
#include "core/interface/configuration_keys.h"
#include "core/nosql_database_provider/src/aws/aws_dynamo_db.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "cpio/client_providers/instance_client_provider/test/aws/test_aws_instance_client_provider.h"
#include "cpio/client_providers/metric_client_provider/test/aws/test_aws_metric_client_provider.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/pbs_client/src/pbs_client.h"

#include "test_aws_dynamo_db.h"
#include "test_aws_s3.h"
#include "test_configuration_keys.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::authorization_proxy::mock::MockAuthorizationProxy;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::Uuid;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::cpio::TestAwsMetricClientOptions;
using google::scp::cpio::client_providers::MetricBatchingOptions;
using google::scp::cpio::client_providers::TestAwsInstanceClientProvider;
using google::scp::cpio::client_providers::TestAwsMetricClientProvider;
using google::scp::cpio::client_providers::TestInstanceClientOptions;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Return;

namespace google::scp::pbs {

static constexpr char kAwsIntegrationTestDependencyProvider[] =
    "kAWSIntegrationTestDependencyProvider";

ExecutionResult AwsIntegrationTestDependencyFactory::ReadTestConfigurations() {
  auto execution_result = config_provider_->Get(
      kCloudwatchEndpointOverride, ec2_metadata_endpoint_override_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsIntegrationTestDependencyProvider, kZeroUuid,
              execution_result,
              "Failed to read EC2Metadata endpoint override.");
    return execution_result;
  }
  execution_result = config_provider_->Get(kCloudwatchEndpointOverride,
                                           cloudwatch_endpoint_override_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsIntegrationTestDependencyProvider, kZeroUuid,
              execution_result, "Failed to read Cloudwatch endpoint override.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult AwsIntegrationTestDependencyFactory::Init() noexcept {
  RETURN_IF_FAILURE(AwsDependencyFactory::Init());
  RETURN_IF_FAILURE(ReadTestConfigurations());
  return SuccessExecutionResult();
}

unique_ptr<core::TokenProviderCacheInterface>
AwsIntegrationTestDependencyFactory::ConstructAuthorizationTokenProviderCache(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<core::HttpClientInterface> http_client) noexcept {
  return make_unique<DummyTokenProviderCache>();
}

unique_ptr<core::AuthorizationProxyInterface>
AwsIntegrationTestDependencyFactory::ConstructAuthorizationProxyClient(
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

unique_ptr<core::BlobStorageProviderInterface>
AwsIntegrationTestDependencyFactory::ConstructBlobStorageClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  return make_unique<TestAwsS3Provider>(async_executor, io_async_executor,
                                        config_provider_);
}

unique_ptr<core::NoSQLDatabaseProviderInterface>
AwsIntegrationTestDependencyFactory::ConstructNoSQLDatabaseClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    core::AsyncPriority async_execution_priority,
    core::AsyncPriority io_async_execution_priority) noexcept {
  return make_unique<TestAwsDynamoDB>(async_executor, io_async_executor,
                                      config_provider_);
}

unique_ptr<cpio::MetricClientInterface>
AwsIntegrationTestDependencyFactory::ConstructMetricClient(
    shared_ptr<core::AsyncExecutorInterface> async_executor,
    shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    shared_ptr<cpio::client_providers::InstanceClientProviderInterface>
        instance_client_provider) noexcept {
  auto metric_client_options = make_shared<TestAwsMetricClientOptions>();
  metric_client_options->cloud_watch_endpoint_override =
      make_shared<string>(cloudwatch_endpoint_override_);
  auto metric_batching_options = make_shared<MetricBatchingOptions>();
  metric_batching_options->metric_namespace = metrics_namespace_;
  metric_batching_options->enable_batch_recording = metrics_batch_push_enabled_;
  // Creates TestAwsMetricClientProvider.
  return make_unique<TestAwsMetricClientProvider>(
      metric_client_options, instance_client_provider, async_executor,
      io_async_executor, metric_batching_options);
}

unique_ptr<cpio::client_providers::InstanceClientProviderInterface>
AwsIntegrationTestDependencyFactory::ConstructInstanceMetadataClient(
    std::shared_ptr<core::HttpClientInterface> http1_client,
    std::shared_ptr<core::HttpClientInterface> http2_client,
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    std::shared_ptr<core::AsyncExecutorInterface> io_async_executor,
    std::shared_ptr<cpio::client_providers::AuthTokenProviderInterface>
        auth_token_provider) noexcept {
  auto options = make_shared<TestInstanceClientOptions>();
  options->region = cloud_service_region_;
  // Mock instance_id and private_ipv4_address for lease manager.
  options->instance_id = "1111";
  options->private_ipv4_address = "111.111.111.111";
  return make_unique<TestAwsInstanceClientProvider>(options);
}

}  // namespace google::scp::pbs
