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

#include "async_context.h"
#include "http_types.h"
#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {

/// Provides methods for interactions with HTTP servers.
class HttpClientInterface : public ServiceInterface {
 public:
  virtual ~HttpClientInterface() = default;

  /**
   * @brief Performs a HTTP request.
   *
   * @param context the context of HTTP action.
   * @return ExecutionResult the execution result of the action.
   */
  virtual ExecutionResult PerformRequest(
      AsyncContext<HttpRequest, HttpResponse>& context) noexcept = 0;
};
}  // namespace google::scp::core
