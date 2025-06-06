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

#include "cc/core/http2_client/src/http_connection.h"

namespace privacy_sandbox::pbs_common {
class MockHttpConnection : public HttpConnection {
 public:
  using HttpConnection::HttpConnection;

  void CancelPendingCallbacks() noexcept {
    HttpConnection::CancelPendingCallbacks();
  }

  void SetIsDropped() { is_dropped_ = true; }

  void SetIsNotDropped() { is_dropped_ = false; }

  void SetIsNotReady() { is_ready_ = false; }

  void SetIsReady() { is_ready_ = true; }

  auto& GetPendingNetworkCallbacks() { return pending_network_calls_; }
};
}  // namespace privacy_sandbox::pbs_common
