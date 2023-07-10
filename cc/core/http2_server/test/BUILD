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

cc_test(
    name = "http2_server_test",
    size = "small",
    srcs = ["http2_server_test.cc"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "//cc/core/http2_server/test/certs:csr.conf",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/authorization_proxy/mock:core_authorization_proxy_mock",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/http2_server/mock:core_http2_server_mock",
        "//cc/core/http2_server/src:core_http2_server_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/metric_client_provider/mock:metric_client_provider_mock",
        "@com_google_googletest//:gtest_main",
    ],
)
