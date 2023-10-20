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
    name = "pbs_front_end_service_test",
    size = "small",
    srcs = glob(["*.cc"]),
    copts = [
        "-std=c++17",
    ],
    linkopts = [
        "-latomic",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/http2_server/mock:core_http2_server_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/transaction_manager/mock:core_transaction_manager_mock",
        "//cc/pbs/budget_key_provider/mock:pbs_budget_key_provider_mock",
        "//cc/pbs/front_end_service/mock:pbs_front_end_service_mock",
        "//cc/pbs/front_end_service/src:pbs_front_end_service_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/pbs/partition_request_router/mock:transaction_request_router_mock",
        "//cc/pbs/transactions/mock:pbs_transactions_mock",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "//cc/public/cpio/utils/metric_aggregation/interface:metric_aggregation_interface",
        "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
        "@com_google_googletest//:gtest_main",
    ],
)
