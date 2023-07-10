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

#include "core/interface/credentials_provider_interface.h"

namespace google::scp::core::credentials_provider::mock {

class MockAwsCredentialsProvider : public CredentialsProviderInterface {
 public:
  MockAwsCredentialsProvider() {}

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult GetCredentials(
      AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
          get_credentials_context) noexcept override {
    if (fail_credentials) {
      get_credentials_context.result = FailureExecutionResult(123);
      get_credentials_context.Finish();
      return FailureExecutionResult(123);
    }

    get_credentials_context.response =
        std::make_shared<GetCredentialsResponse>();
    get_credentials_context.response->access_key_id =
        std::make_shared<std::string>("access_key_id");
    get_credentials_context.response->access_key_secret =
        std::make_shared<std::string>("access_key_secret");
    get_credentials_context.response->security_token =
        std::make_shared<std::string>("security_token");
    get_credentials_context.result = SuccessExecutionResult();
    get_credentials_context.Finish();
    return SuccessExecutionResult();
  }

  bool fail_credentials = false;
};
}  // namespace google::scp::core::credentials_provider::mock
