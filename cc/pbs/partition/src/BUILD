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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "pbs_partition_lib",
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
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/transaction_manager/src:core_transaction_manager_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/pbs/budget_key_provider/src:pbs_budget_key_provider_lib",
        "//cc/pbs/checkpoint_service/src:pbs_checkpoint_service_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/public/cpio/interface/metric_client",
        "@com_google_absl//absl/strings",
        "@nlohmann_json//:lib",
    ],
)
