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

#include "pbs_instance_multi_partition_platform_wrapper.h"

#include <memory>
#include <utility>
#include <vector>

#include "pbs_instance_multi_partition.h"

#if defined(PBS_GCP)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/gcp_dependency_factory.h"
#elif defined(PBS_GCP_INTEGRATION_TEST)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/gcp_integration_test/gcp_integration_test_dependency_factory.h"
#elif defined(PBS_AWS)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/aws/aws_dependency_factory.h"
#elif defined(PBS_AWS_INTEGRATION_TEST)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/aws_integration_test/aws_integration_test_dependency_factory.h"
#elif defined(PBS_LOCAL)
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_dependency_factory.h"
#endif

using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;

namespace google::scp::pbs {

PBSInstanceMultiPartitionPlatformWrapper::
    PBSInstanceMultiPartitionPlatformWrapper(
        const shared_ptr<ConfigProviderInterface> config_provider)
    : config_provider_(config_provider) {}

ExecutionResult PBSInstanceMultiPartitionPlatformWrapper::Init() noexcept {
#if defined(PBS_GCP)
  SCP_DEBUG(kPBSInstance, core::common::kZeroUuid, "Running GCP Instance.");
  auto platform_dependency_factory =
      make_unique<GcpDependencyFactory>(config_provider_);
#elif defined(PBS_GCP_INTEGRATION_TEST)
  SCP_DEBUG(kPBSInstance, core::common::kZeroUuid,
            "Running GCP Integration Test Instance.");
  auto platform_dependency_factory =
      make_unique<GcpIntegrationTestDependencyFactory>(config_provider_);
#elif defined(PBS_AWS)
  SCP_DEBUG(kPBSInstance, core::common::kZeroUuid, "Running AWS Instance.");
  auto platform_dependency_factory =
      make_unique<AwsDependencyFactory>(config_provider_);
#elif defined(PBS_AWS_INTEGRATION_TEST)
  SCP_DEBUG(kPBSInstance, core::common::kZeroUuid,
            "Running AWS Integration Test Instance.");
  auto platform_dependency_factory =
      make_unique<AwsIntegrationTestDependencyFactory>(config_provider_);
#elif defined(PBS_LOCAL)
  SCP_DEBUG(kPBSInstance, core::common::kZeroUuid, "Running Local Instance.");
  auto platform_dependency_factory =
      make_unique<LocalDependencyFactory>(config_provider_);
#endif
  RETURN_IF_FAILURE(platform_dependency_factory->Init());
  pbs_instance_ = make_shared<PBSInstanceMultiPartition>(
      config_provider_, move(platform_dependency_factory));
  return pbs_instance_->Init();
}

ExecutionResult PBSInstanceMultiPartitionPlatformWrapper::Run() noexcept {
  return pbs_instance_->Run();
}

ExecutionResult PBSInstanceMultiPartitionPlatformWrapper::Stop() noexcept {
  return pbs_instance_->Stop();
}
}  // namespace google::scp::pbs
