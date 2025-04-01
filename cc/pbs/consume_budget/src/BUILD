# Copyright 2025 Google LLC
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

package(default_visibility = ["//cc:pbs_visibility"])

cc_library(
    name = "budget_consumer",
    hdrs = ["budget_consumer.h"],
    deps = [
        "//cc/core/interface:interface_lib",
        "//cc/public/core/interface:execution_result",
    ],
)

cc_library(
    name = "binary_budget_consumer",
    srcs = ["binary_budget_consumer.cc"],
    hdrs = ["binary_budget_consumer.h"],
    deps = [
        ":budget_consumer",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "//cc/pbs/consume_budget/src/gcp:error_codes",
        "//cc/pbs/front_end_service/src:front_end_utils",
        "//cc/pbs/proto/storage:budget_value_cc_proto",
        "//cc/public/core/interface:errors",
        "//cc/public/core/interface:execution_result",
        "@com_github_googleapis_google_cloud_cpp//:spanner",
        "@com_github_nlohmann_json//:singleheader-json",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)
