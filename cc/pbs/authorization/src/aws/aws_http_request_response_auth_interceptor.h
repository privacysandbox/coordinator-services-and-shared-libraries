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

#include <string>

#include "core/interface/authorization_proxy_interface.h"
#include "core/interface/http_request_response_auth_interceptor_interface.h"
#include "core/interface/http_types.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs {

class AwsHttpRequestResponseAuthInterceptor
    : public core::HttpRequestResponseAuthInterceptorInterface {
 public:
  explicit AwsHttpRequestResponseAuthInterceptor(const std::string& aws_region)
      : aws_region_(aws_region) {}

  core::ExecutionResult PrepareRequest(
      const core::AuthorizationMetadata& authorization_metadata,
      core::HttpRequest& http_request) override;

  core::ExecutionResultOr<core::AuthorizedMetadata>
  ObtainAuthorizedMetadataFromResponse(
      const core::AuthorizationMetadata& authorization_metadata,
      const core::HttpResponse& http_response) override;

 private:
  std::string aws_region_;
};

}  // namespace google::scp::pbs
