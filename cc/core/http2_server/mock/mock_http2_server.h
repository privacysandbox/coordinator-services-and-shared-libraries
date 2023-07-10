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

#include "core/interface/http_server_interface.h"

namespace google::scp::core::http2_server::mock {

class MockHttp2Server : public core::HttpServerInterface {
 public:
  MockHttp2Server() {}

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult RegisterResourceHandler(
      HttpMethod http_method, std::string& resource_path,
      HttpHandler& handler) noexcept override {
    return SuccessExecutionResult();
  }
};
}  // namespace google::scp::core::http2_server::mock
