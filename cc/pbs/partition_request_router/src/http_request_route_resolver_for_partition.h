/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <string>

#include "core/interface/config_provider_interface.h"
#include "core/interface/http_request_route_resolver_interface.h"
#include "core/interface/partition_manager_interface.h"
#include "core/interface/partition_namespace_interface.h"
#include "core/interface/partition_types.h"
#include "pbs/interface/pbs_partition_interface.h"
#include "pbs/interface/pbs_partition_manager_interface.h"

namespace google::scp::pbs {
/**
 * @brief This implementation of HttpRequestRouteResolverInterface allows the
 * accepted HTTP requests of PBS to be routed based on the returned
 * RequestRouteEndpointInfo.
 *
 * Request's headers such as ReportingOrigin and ClaimedIdentity are used in
 * determining the endpoint which hosts the target partition to process the
 * request, and the returned RequestRouteEndpointInfo reflects that.
 *
 */
class HttpRequestRouteResolverForPartition
    : public core::HttpRequestRouteResolverInterface {
 public:
  HttpRequestRouteResolverForPartition(
      const std::shared_ptr<core::PartitionNamespaceInterface>&
          partition_namespace,
      const std::shared_ptr<PBSPartitionManagerInterface>& partition_manager,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : partition_namespace_(partition_namespace),
        partition_manager_(partition_manager),
        config_provider_(config_provider) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResultOr<core::RequestRouteEndpointInfo> ResolveRoute(
      const core::HttpRequest& request) noexcept override;

 protected:
  /**
   * @brief Resource ID in PBS's case is ReportingOrigin which are mapped onto
   * the partition space.
   *
   * @param request
   * @return core::ExecutionResultOr<core::ResourceId>
   */
  core::ExecutionResultOr<core::ResourceId> ExtractResourceId(
      const core::HttpRequest& request) const;

  /// @brief Namespace of the partition to which request will be mapped to.
  const std::shared_ptr<core::PartitionNamespaceInterface> partition_namespace_;

  /// @brief Partition object to be retrieved from
  const std::shared_ptr<PBSPartitionManagerInterface> partition_manager_;

  /// @brief Remote coordinator claimed identity.
  std::string remote_coordinator_claimed_identity_;

  /// @brief Config provider
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
};
}  // namespace google::scp::pbs
