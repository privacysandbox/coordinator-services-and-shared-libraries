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

#include <gtest/gtest.h>

#include <memory>

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/config_provider/src/env_config_provider.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_dependency_factory.h"

namespace google::scp::pbs {
namespace {

using ::google::scp::core::AsyncExecutor;
using ::google::scp::core::EnvConfigProvider;
using ::google::scp::pbs::LocalDependencyFactory;

TEST(PBSInstanceV3Test, TestInitRunAndStopWithLocalDependencyFactory) {
  const bool should_overwrite = true;
  setenv(kAsyncExecutorQueueSize, "10000", should_overwrite);
  setenv(kAsyncExecutorThreadsCount, "10", should_overwrite);
  setenv(kIOAsyncExecutorQueueSize, "10000", should_overwrite);
  setenv(kIOAsyncExecutorThreadsCount, "10", should_overwrite);
  setenv(kTransactionManagerCapacity, "10000", should_overwrite);
  setenv(kJournalServiceBucketName, "bucket", should_overwrite);
  setenv(kJournalServicePartitionName, "00000000-0000-0000-0000-000000000000",
         should_overwrite);
  setenv(kPrivacyBudgetServiceHostAddress, "localhost", should_overwrite);
  setenv(kPrivacyBudgetServiceHostPort, "8000", should_overwrite);
  setenv(kPrivacyBudgetServiceHealthPort, "8001", should_overwrite);
  setenv(kPrivacyBudgetServiceExternalExposedHostPort, "8000",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceCloudServiceRegion, "region",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceAuthServiceEndpoint, "https://auth.com",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceClaimedIdentity, "remote-id",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceAssumeRoleArn, "arn", should_overwrite);
  setenv(kRemotePrivacyBudgetServiceAssumeRoleExternalId, "id",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceHostAddress, "https://remote.com",
         should_overwrite);
  setenv(kAuthServiceEndpoint, "https://auth.com", should_overwrite);
  setenv(core::kCloudServiceRegion, "region", should_overwrite);
  setenv(kServiceMetricsNamespace, "ns", should_overwrite);
  setenv(kTotalHttp2ServerThreadsCount, "10", should_overwrite);
  setenv(kPBSPartitionLockTableNameConfigName, "partition_lock_table",
         should_overwrite);
  setenv(kOtelEnabled, "true", should_overwrite);
  std::shared_ptr<EnvConfigProvider> config_provider =
      std::make_shared<EnvConfigProvider>();
  std::unique_ptr<LocalDependencyFactory> platform_dependency_factory =
      std::make_unique<LocalDependencyFactory>(config_provider);
  PBSInstanceV3 pbs_instance_v3(config_provider,
                                std::move(platform_dependency_factory));
  // TODO: Uncomment the following once budget_consumption_helper is created
  // correctly.
  // EXPECT_TRUE(pbs_instance_v3.Init());
  // EXPECT_TRUE(pbs_instance_v3.Run());
  // EXPECT_TRUE(pbs_instance_v3.Stop());
}

}  // namespace
}  // namespace google::scp::pbs
