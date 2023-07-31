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

namespace google::scp::core::http2_client::mock {
class MockHttpClient : public HttpClientInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult PerformRequest(
      AsyncContext<HttpRequest, HttpResponse>& context) noexcept {
    if (perform_request_mock) {
      return perform_request_mock(context);
    }

    if (!http_get_result_mock.Successful()) {
      context.result = http_get_result_mock;
      context.Finish();
      return SuccessExecutionResult();
    }

    if (*request_mock.path == *context.request->path) {
      context.response = std::make_shared<HttpResponse>(response_mock);
      context.result = SuccessExecutionResult();
    }

    context.Finish();
    return SuccessExecutionResult();
  }

  HttpRequest request_mock;
  HttpResponse response_mock;
  ExecutionResult http_get_result_mock = SuccessExecutionResult();
  std::function<ExecutionResult(AsyncContext<HttpRequest, HttpResponse>&)>
      perform_request_mock;
};
}  // namespace google::scp::core::http2_client::mock
