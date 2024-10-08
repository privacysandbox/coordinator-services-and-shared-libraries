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

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "front_end_service_test",
    srcs = ["front_end_service_test.cc"],
    linkopts = [
        "-latomic",
    ],
    deps = [
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/http2_server/mock:core_http2_server_mock",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/mock:telemetry_mock",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/front_end_service/mock:pbs_front_end_service_mock",
        "//cc/pbs/partition_request_router/mock:transaction_request_router_mock",
        "//cc/pbs/transactions/mock:pbs_transactions_mock",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "front_end_service_utils_test",
    srcs = ["front_end_service_utils_test.cc"],
    linkopts = [
        "-latomic",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/front_end_service/mock:pbs_front_end_service_mock",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "transaction_request_router_test",
    srcs = ["transaction_request_router_test.cc"],
    linkopts = [
        "-latomic",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/core/transaction_manager/mock:core_transaction_manager_mock",
        "//cc/pbs/front_end_service/mock:pbs_front_end_service_mock",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "front_end_service_v2_test",
    srcs = ["front_end_service_v2_test.cc"],
    linkopts = [
        "-latomic",
    ],
    deps = [
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/http2_server/mock:core_http2_server_mock",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/mock:telemetry_mock",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/telemetry/src/common:telemetry_metric_utils",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/transaction_manager/mock:core_transaction_manager_mock",
        "//cc/pbs/front_end_service/mock:pbs_front_end_service_mock",
        "//cc/pbs/front_end_service/src:error_codes",
        "//cc/pbs/front_end_service/src:front_end_service_v2",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/pbs/partition_request_router/mock:transaction_request_router_mock",
        "//cc/pbs/transactions/mock:pbs_transactions_mock",
        "//cc/public/cpio/utils/metric_aggregation/mock:metric_aggregation_mock",
        "@com_google_googletest//:gtest_main",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)
