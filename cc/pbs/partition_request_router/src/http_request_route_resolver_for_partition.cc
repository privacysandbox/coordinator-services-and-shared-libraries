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

#include "pbs/partition_request_router/src/http_request_route_resolver_for_partition.h"

#include "core/interface/partition_types.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/type_def.h"
#include "pbs/partition_request_router/src/error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpRequest;
using google::scp::core::RequestRouteEndpointInfo;
using google::scp::core::ResourceId;
using google::scp::core::SuccessExecutionResult;

namespace google::scp::pbs {

ExecutionResult HttpRequestRouteResolverForPartition::Init() noexcept {
  return config_provider_->Get(kRemotePrivacyBudgetServiceClaimedIdentity,
                               remote_coordinator_claimed_identity_);
}

ExecutionResultOr<ResourceId>
HttpRequestRouteResolverForPartition::ExtractResourceId(
    const HttpRequest& request) const {
  if (!request.headers) {
    return FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID);
  }

  // Resource identifier is the request's claimed identity i.e. reporting
  // origin.
  auto header_it =
      request.headers->find(std::string(core::kClaimedIdentityHeader));
  if (header_it == request.headers->end()) {
    return core::FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID);
  }

  // If this is a remote coordinator's request, look at the transaction origin
  // for the resource identifier instead.
  if (header_it->second == remote_coordinator_claimed_identity_) {
    auto origin_header_it =
        request.headers->find(std::string(kTransactionOriginHeader));
    if (origin_header_it == request.headers->end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID);
    }
    return origin_header_it->second;
  }

  return header_it->second;
}

ExecutionResultOr<RequestRouteEndpointInfo>
HttpRequestRouteResolverForPartition::ResolveRoute(
    const HttpRequest& request) noexcept {
  auto resource_id_or = ExtractResourceId(request);
  if (!resource_id_or.Successful()) {
    return FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID);
  }
  auto partition_id =
      partition_namespace_->MapResourceToPartition(*resource_id_or);
  auto partition_address_or =
      partition_manager_->GetPartitionAddress(partition_id);
  if (!partition_address_or.Successful()) {
    return FailureExecutionResult(
        core::errors::SC_PBS_TRANSACTION_REQUEST_ROUTER_PARTITION_UNAVAILABLE);
  }
  bool is_local_endpoint =
      (*partition_address_or.value() == core::kLocalPartitionAddressUri);
  return RequestRouteEndpointInfo(*partition_address_or, is_local_endpoint);
}
}  // namespace google::scp::pbs
