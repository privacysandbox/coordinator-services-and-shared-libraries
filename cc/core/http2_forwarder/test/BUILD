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

load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "http2_forwarder_test",
    size = "small",
    srcs = [
        "http2_forwarder_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    data = [
        "//cc/core/http2_server/test/certs:csr.conf",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/http2_forwarder/src:core_http2_forwarder_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_github_nghttp2_nghttp2//:nghttp2",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
