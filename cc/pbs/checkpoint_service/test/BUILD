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

TEST_SRCS = [
    "checkpoint_service_test.cc",
]

cc_test(
    name = "pbs_checkpoint_service_test",
    size = "small",
    srcs = TEST_SRCS,
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/blob_storage_provider/mock:blob_storage_provider_mock",
        "//cc/core/common/serialization/src:serialization_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/transaction_manager/mock:core_transaction_manager_mock",
        "//cc/pbs/budget_key/mock:pbs_budget_key_mock",
        "//cc/pbs/budget_key_provider/mock:pbs_budget_key_provider_mock",
        "//cc/pbs/checkpoint_service/mock:pbs_checkpoint_service_mock",
        "//cc/pbs/checkpoint_service/src:pbs_checkpoint_service_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_google_googletest//:gtest_main",
    ],
)
