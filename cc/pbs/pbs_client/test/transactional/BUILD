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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "test_pbs_transactional_client_lib",
    testonly = True,
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/transaction_manager/mock:core_transaction_manager_mock",
        "//cc/core/transaction_manager/src:core_transaction_manager_lib",
        "//cc/pbs/front_end_service/src:pbs_front_end_service_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/pbs/pbs_client/mock:pbs_client_mock",
        "//cc/pbs/pbs_client/mock/transactional:pbs_transactional_client_mock",
        "//cc/pbs/pbs_client/src:pbs_client_lib",
        "//cc/pbs/pbs_client/src/transactional:pbs_transactional_client_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
    ],
)
