# Copyright 2023 Google LLC
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

load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "gcp_http_request_response_auth_interceptor_test",
    srcs = ["gcp_http_request_response_auth_interceptor_test.cc"],
    deps = [
        "//cc/core/authorization_service/src:core_authorization_service",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/pbs/authorization/src/gcp:gcp_http_request_response_auth_interceptor",
        "//cc/public/core/interface:execution_result",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:lib",
    ],
)
