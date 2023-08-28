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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "pbs_checkpoint_service_lib",
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
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/transaction_manager/interface:core_transaction_manager_interface_lib",
        "//cc/core/transaction_manager/src:core_transaction_manager_lib",
        "//cc/cpio/client_providers/metric_client_provider/interface:metric_utils_interface_lib",
        "//cc/pbs/budget_key_provider/src:pbs_budget_key_provider_lib",
        "//cc/pbs/transactions/src:pbs_transactions_lib",
    ],
)