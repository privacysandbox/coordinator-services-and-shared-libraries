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
    name = "pbs_budget_key_timeframe_manager_test",
    size = "small",
    srcs = ["budget_key_timeframe_manager_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/src/common:telemetry_metric_utils",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/budget_key/src:pbs_budget_key_lib",
        "//cc/pbs/budget_key_timeframe_manager/mock:pbs_budget_key_timeframe_manager_mock",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_google_googletest//:gtest_main",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)

cc_test(
    name = "pbs_budget_key_timeframe_serialization_test",
    size = "small",
    srcs = ["budget_key_timeframe_serialization_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/common/serialization/src:serialization_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/budget_key/src:pbs_budget_key_lib",
        "//cc/pbs/budget_key_timeframe_manager/mock:pbs_budget_key_timeframe_manager_mock",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "pbs_budget_key_timeframe_utils_test",
    size = "small",
    srcs = ["budget_key_timeframe_utils_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/common/serialization/src:serialization_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/budget_key/src:pbs_budget_key_lib",
        "//cc/pbs/budget_key_timeframe_manager/mock:pbs_budget_key_timeframe_manager_mock",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_google_googletest//:gtest_main",
    ],
)
