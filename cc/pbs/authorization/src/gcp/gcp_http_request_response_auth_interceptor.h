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

#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_request_response_auth_interceptor_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs {

class GcpHttpRequestResponseAuthInterceptor
    : public pbs_common::HttpRequestResponseAuthInterceptorInterface {
 public:
  GcpHttpRequestResponseAuthInterceptor() : config_provider_(nullptr) {}

  explicit GcpHttpRequestResponseAuthInterceptor(
      std::shared_ptr<pbs_common::ConfigProviderInterface> config_provider)
      : config_provider_(config_provider) {}

  pbs_common::ExecutionResult PrepareRequest(
      const pbs_common::AuthorizationMetadata& authorization_metadata,
      pbs_common::HttpRequest& http_request) override;

  pbs_common::ExecutionResultOr<pbs_common::AuthorizedMetadata>
  ObtainAuthorizedMetadataFromResponse(
      const pbs_common::AuthorizationMetadata& authorization_metadata,
      const pbs_common::HttpResponse& http_response) override;

 private:
  std::shared_ptr<pbs_common::ConfigProviderInterface> config_provider_;
};

}  // namespace privacy_sandbox::pbs
