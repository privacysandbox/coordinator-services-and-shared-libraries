// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CC_CORE_HTTP2_CLIENT_SRC_HTTP_CLIENT_DEF
#define CC_CORE_HTTP2_CLIENT_SRC_HTTP_CLIENT_DEF

#include "absl/strings/string_view.h"

namespace google::scp::core {
// Meter
inline constexpr absl::string_view kHttpConnectionPoolMeter =
    "Http Connection Pool";
inline constexpr absl::string_view kHttpConnectionMeter = "Http Connection";

// View
inline constexpr absl::string_view kClientServerLatencyView =
    "Client-Server Latency";
inline constexpr absl::string_view kClientRequestDurationView =
    "Client Request Duration";
inline constexpr absl::string_view kClientConnectionDurationView =
    "Client Connection Duration";

// Metrics
inline constexpr absl::string_view kClientActiveRequestsMetric =
    "http.client.active_requests";
inline constexpr absl::string_view kClientOpenConnectionsMetric =
    "http.client.open_connections";
inline constexpr absl::string_view kClientAddressErrorsMetric =
    "http.client.address_errors";
inline constexpr absl::string_view kClientConnectErrorsMetric =
    "http.client.connect_errors";
inline constexpr absl::string_view kClientServerLatencyMetric =
    "http.client.server_latency";
inline constexpr absl::string_view kClientRequestDurationMetric =
    "http.client.request.duration";
inline constexpr absl::string_view kClientRequestBodySizeMetric =
    "http.client.request.body.size";
inline constexpr absl::string_view kClientResponseBodySizeMetric =
    "http.client.response.body.size";
inline constexpr absl::string_view kClientResponseMetric =
    "http.client.response";
inline constexpr absl::string_view kClientConnectionDurationMetric =
    "http.client.connection.duration";

// Labels
inline constexpr absl::string_view kUriLabel = "server.uri";
inline constexpr absl::string_view kResponseStatusLabel =
    "server.response_status";
inline constexpr absl::string_view kClientReturnCodeLabel =
    "client.return_code";

}  // namespace google::scp::core
#endif  // CC_CORE_HTTP2_CLIENT_SRC_HTTP_CLIENT_DEF
