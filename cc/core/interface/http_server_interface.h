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

#include <functional>
#include <string>

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/service_interface.h"

namespace privacy_sandbox::pbs_common {

/// Type definition for the resource handler.
using HttpHandler =
    std::function<ExecutionResult(AsyncContext<HttpRequest, HttpResponse>&)>;

/// Provides HTTP(S) server functionality.
class HttpServerInterface : public ServiceInterface {
 public:
  virtual ~HttpServerInterface() = default;

  /**
   * @brief Registers resource handler for http operations.
   *
   * @param http_method The method of the operation.
   * @param resource_path The resource path in REST format.
   * @param handler The handler of the specific path.
   * @return ExecutionResult
   */
  virtual ExecutionResult RegisterResourceHandler(
      HttpMethod http_method, std::string& resource_path,
      HttpHandler& handler) noexcept = 0;
};
}  // namespace privacy_sandbox::pbs_common
