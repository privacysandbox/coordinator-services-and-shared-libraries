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
#include "authorization_proxy_interface.h"
#include "http_types.h"
#include "service_interface.h"

namespace google::scp::core {
/**
 * @brief Helper class to build http headers and parse http response body
 */
class HttpRequestResponseAuthInterceptorInterface {
 public:
  virtual ~HttpRequestResponseAuthInterceptorInterface() = default;

  /**
   * @brief Prepares the request for interacting with the cloud platform which
   * may include adding authorization related headers to the headers map.
   *
   * @return ExecutionResult
   */
  virtual ExecutionResult PrepareRequest(const AuthorizationMetadata&,
                                         HttpRequest&) = 0;

  /**
   * @brief Parse response to obtain authorization related data
   *
   * @return ExecutionResultOr<AuthorizedMetadata>
   */
  virtual ExecutionResultOr<AuthorizedMetadata>
  ObtainAuthorizedMetadataFromResponse(const AuthorizationMetadata&,
                                       const HttpResponse&) = 0;
};
}  // namespace google::scp::core
