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
    name = "core_transaction_manager_lib",
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
        "//cc/core/common/auto_expiry_concurrent_map/src:auto_expiry_concurrent_map_lib",
        "//cc/core/common/concurrent_queue/src:concurrent_queue_lib",
        "//cc/core/common/serialization/src:serialization_lib",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/transaction_manager/interface:core_transaction_manager_interface_lib",
        "//cc/core/transaction_manager/src/proto:core_transaction_manager_proto_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/public/cpio/utils/metric_aggregation/interface:metric_aggregation_interface",
        "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
        "//cc/public/cpio/utils/metric_aggregation/src:metric_aggregation",
    ],
)
