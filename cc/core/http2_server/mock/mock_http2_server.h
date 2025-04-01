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

#include "cc/core/interface/http_server_interface.h"

namespace privacy_sandbox::pbs_common {

class MockHttp2Server : public HttpServerInterface {
 public:
  MockHttp2Server() {}

  google::scp::core::ExecutionResult Init() noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Run() noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Stop() noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult RegisterResourceHandler(
      HttpMethod http_method, std::string& resource_path,
      HttpHandler& handler) noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }
};
}  // namespace privacy_sandbox::pbs_common
