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
    name = "pbs_front_end_service_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    linkopts = [
        "-latomic",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/metric_client_provider/interface:metric_utils_interface_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/cpio/client_providers/metric_client_provider/src/utils:metric_utils_lib",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/pbs/transactions/src:pbs_transactions_lib",
        "@com_google_absl//absl/container:flat_hash_map",
        "@nlohmann_json//:lib",
    ],
)