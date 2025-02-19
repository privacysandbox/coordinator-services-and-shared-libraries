# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//cc:pbs_visibility"])

cc_library(
    name = "core_http2_server_lib",
    srcs = glob(
        [
            "**/*.cc",
            "**/*.h",
        ],
    ),
    deps = [
        "//cc/core/authorization_proxy/src:core_authorization_proxy_lib",
        "//cc/core/authorization_service/src:core_authorization_service",
        "//cc/core/interface:interface_lib",
        "@boost//:asio_ssl",
        "@boost//:system",
        "@com_github_nghttp2_nghttp2//:nghttp2",
        "@com_github_nlohmann_json//:singleheader-json",
        "@com_google_absl//absl/strings",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)
