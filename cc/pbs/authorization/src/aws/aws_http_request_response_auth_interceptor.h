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

#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_request_response_auth_interceptor_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/public/core/interface/execution_result.h"

namespace google::scp::pbs {

class AwsHttpRequestResponseAuthInterceptor
    : public privacy_sandbox::pbs_common::
          HttpRequestResponseAuthInterceptorInterface {
 public:
  explicit AwsHttpRequestResponseAuthInterceptor(const std::string& aws_region)
      : aws_region_(aws_region) {}

  AwsHttpRequestResponseAuthInterceptor(
      const std::string& aws_region,
      std::shared_ptr<privacy_sandbox::pbs_common::ConfigProviderInterface>
          config_provider)
      : aws_region_(aws_region) {}

  core::ExecutionResult PrepareRequest(
      const privacy_sandbox::pbs_common::AuthorizationMetadata&
          authorization_metadata,
      privacy_sandbox::pbs_common::HttpRequest& http_request) override;

  core::ExecutionResultOr<privacy_sandbox::pbs_common::AuthorizedMetadata>
  ObtainAuthorizedMetadataFromResponse(
      const privacy_sandbox::pbs_common::AuthorizationMetadata&
          authorization_metadata,
      const privacy_sandbox::pbs_common::HttpResponse& http_response)
      override;

 private:
  std::string aws_region_;
};

}  // namespace google::scp::pbs
