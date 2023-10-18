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

#include "core/interface/http_client_interface.h"
#include "core/interface/http_request_router_interface.h"

namespace google::scp::core {

class Http2Forwarder : public HttpRequestRouterInterface {
 public:
  explicit Http2Forwarder(
      const std::shared_ptr<HttpClientInterface>& http2_client);

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult RouteRequest(
      AsyncContext<HttpRequest, HttpResponse>& context) noexcept override;

 protected:
  void OnHttpResponse(
      AsyncContext<HttpRequest, HttpResponse>& original_context,
      AsyncContext<HttpRequest, HttpResponse>& context) noexcept;

  std::shared_ptr<HttpClientInterface> http2_client_;
};

}  // namespace google::scp::core
