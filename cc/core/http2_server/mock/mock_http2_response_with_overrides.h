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

#include "core/http2_server/src/http2_response.h"

namespace google::scp::core::http2_server::mock {

class MockNgHttp2ResponseWithOverrides : public NgHttp2Response {
 public:
  explicit MockNgHttp2ResponseWithOverrides(
      const nghttp2::asio_http2::server::response& ng2_response)
      : NgHttp2Response(ng2_response) {}
};

}  // namespace google::scp::core::http2_server::mock
