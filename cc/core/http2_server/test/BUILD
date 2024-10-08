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

# For running these tests in parallel use,
# bazel test  --config=asan -c dbg //cc/core/http2_server/test:http2_server_test --runs_per_test=10 --test_filter=Http2ServerTest.TestServerDurationMetric --jobs=1
cc_test(
    name = "http2_server_test",
    size = "small",
    srcs = [
        "http2_server_request_routing_test.cc",
        "http2_server_test.cc",
    ],
    data = [
        "//cc/core/http2_server/test/certs:csr.conf",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/authorization_proxy/mock:core_authorization_proxy_mock",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/http2_forwarder/mock:core_http2_forwarder_mock",
        "//cc/core/http2_server/mock:core_http2_server_mock",
        "//cc/core/http2_server/src:core_http2_server_lib",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/mock:telemetry_mock",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/telemetry/src/common:telemetry_metric_utils",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)

cc_test(
    name = "http2_server_load_test",
    size = "large",
    srcs = [
        "http2_server_load_test.cc",
    ],
    data = [
        "//cc/core/http2_server/test/certs:csr.conf",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/authorization_proxy/mock:core_authorization_proxy_mock",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/http2_forwarder/mock:core_http2_forwarder_mock",
        "//cc/core/http2_server/mock:core_http2_server_mock",
        "//cc/core/http2_server/src:core_http2_server_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "//cc/public/cpio/utils/metric_aggregation/interface:metric_aggregation_interface",
        "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
        "@com_google_googletest//:gtest_main",
    ],
)
