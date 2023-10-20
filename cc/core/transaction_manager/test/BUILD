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
    name = "transaction_phase_manager_test",
    size = "small",
    srcs = ["transaction_phase_manager_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/transaction_manager/interface:core_transaction_manager_interface_lib",
        "//cc/core/transaction_manager/src:core_transaction_manager_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "transaction_engine_test",
    size = "small",
    srcs = glob([
        "transaction_engine_test.cc",
        "transaction_engine_remote_test.cc",
        "transaction_engine_local_and_remote_test.cc",
    ]),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/blob_storage_provider/mock:blob_storage_provider_mock",
        "//cc/core/common/proto:core_common_proto_lib",
        "//cc/core/common/serialization/src:serialization_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/interface:logger_interface",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/lease_manager/src:core_lease_manager_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/transaction_manager/interface:core_transaction_manager_interface_lib",
        "//cc/core/transaction_manager/mock:core_transaction_manager_mock",
        "//cc/core/transaction_manager/src:core_transaction_manager_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "transaction_manager_test",
    size = "small",
    srcs = ["transaction_manager_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/transaction_manager/interface:core_transaction_manager_interface_lib",
        "//cc/core/transaction_manager/mock:core_transaction_manager_mock",
        "//cc/core/transaction_manager/src:core_transaction_manager_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_google_googletest//:gtest_main",
    ],
)
