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

#include "core/http2_client/src/http_connection.h"

namespace google::scp::core::http2_client::mock {
class MockHttpConnection : public HttpConnection {
 public:
  MockHttpConnection(
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      const std::string& host, const std::string& service, bool https)
      : HttpConnection(async_executor, host, service, https) {}

  void CancelPendingCallbacks() noexcept {
    HttpConnection::CancelPendingCallbacks();
  }

  auto& GetPendingNetworkCallbacks() { return pending_network_calls_; }
};
}  // namespace google::scp::core::http2_client::mock
