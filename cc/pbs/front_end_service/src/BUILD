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

cc_library(
    name = "error_codes",
    hdrs = ["error_codes.h"],
    visibility = ["//cc/pbs/front_end_service:__subpackages__"],
)

cc_library(
    name = "front_end_utils",
    srcs = ["front_end_utils.cc"],
    hdrs = ["front_end_utils.h"],
    visibility = ["//cc:pbs_visibility"],
    deps = [
        ":error_codes",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "@com_github_nlohmann_json//:singleheader-json",
        "@com_google_absl//absl/container:flat_hash_map",
        "@libpsl",
    ],
)

cc_library(
    name = "front_end_service_v2",
    srcs = ["front_end_service_v2.cc"],
    hdrs = ["front_end_service_v2.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":error_codes",
        ":front_end_utils",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "//cc/pbs/consume_budget/src:binary_budget_consumer",
        "//cc/pbs/consume_budget/src/gcp:consume_budget",
        "//cc/pbs/consume_budget/src/gcp:error_codes",
        "//cc/pbs/interface:pbs_interface_lib",
        "@com_github_nlohmann_json//:singleheader-json",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)
