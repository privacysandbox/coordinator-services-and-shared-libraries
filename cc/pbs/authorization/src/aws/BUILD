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
    name = "aws_http_request_response_auth_interceptor",
    srcs = ["aws_http_request_response_auth_interceptor.cc"],
    hdrs = ["aws_http_request_response_auth_interceptor.h"],
    deps = [
        "//cc/core/authorization_service/src:core_authorization_service",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/http2_client/src/aws:aws_v4_signer",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/public/core/interface:execution_result",
        "@com_github_nlohmann_json//:singleheader-json",
        "@com_google_absl//absl/strings",
    ],
)
