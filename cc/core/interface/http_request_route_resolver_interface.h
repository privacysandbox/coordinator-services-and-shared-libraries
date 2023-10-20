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

#include "core/interface/http_types.h"
#include "core/interface/initializable_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core {

/**
 * @brief RequestRouteEndpointInfo for describing the an endpoint that
 * can process the request.
 *
 * By default, this structure represents a local endpoint, for which the address
 * is optionally provided in the structure. if the URI is not present for the
 * local endpoint, the caller is expected to know the local endpoint URI in some
 * other means.
 */
struct RequestRouteEndpointInfo {
  // Default is a local endpoint
  RequestRouteEndpointInfo() : is_local_endpoint(true) {}

  RequestRouteEndpointInfo(const std::shared_ptr<Uri>& uri,
                           bool is_local_endpoint)
      : uri(uri), is_local_endpoint(is_local_endpoint) {}

  /// @brief URI of the endpoint
  const std::shared_ptr<Uri> uri;
  /// @brief If this endpoint is local to this instance or not
  const bool is_local_endpoint;
};

/**
 * @brief Abstraction to resolve target route for a given request.
 */
class HttpRequestRouteResolverInterface : public InitializableInterface {
 public:
  virtual ~HttpRequestRouteResolverInterface() = default;
  /**
   * @brief Resolves a HTTP request's target endpoint information from
   * HTTPRequest's headers/body.
   *
   * @param request
   * @return ExecutionResult
   */
  virtual ExecutionResultOr<RequestRouteEndpointInfo> ResolveRoute(
      const HttpRequest& request) noexcept = 0;
};
}  // namespace google::scp::core
