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
load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

configure_make(
    name = "libpsl",
    configure_in_place = True,
    lib_source = "@libpsl//:all_srcs",
    out_shared_libs = [
        "libpsl.so",
        "libpsl.so.5",
    ],
)

cc_library(
    name = "error_codes",
    hdrs = ["error_codes.h"],
    visibility = ["//cc/pbs/front_end_service:__subpackages__"],
)

cc_library(
    name = "front_end_utils",
    srcs = ["front_end_utils.cc"],
    hdrs = ["front_end_utils.h"],
    deps = [
        ":error_codes",
        ":libpsl",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "@com_google_absl//absl/container:flat_hash_map",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)

cc_library(
    name = "transaction_request_router",
    srcs = ["transaction_request_router.cc"],
    hdrs = ["transaction_request_router.h"],
    deps = [
        "//cc/core/interface:interface_lib",
        "//cc/pbs/interface:pbs_interface_lib",
    ],
)

cc_library(
    name = "metric_initialization",
    srcs = ["metric_initialization.cc"],
    hdrs = ["metric_initialization.h"],
    deps = [
        "//cc/pbs/transactions/src:pbs_transactions_lib",
        "//cc/public/cpio/utils/metric_aggregation/src:metric_aggregation",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "front_end_service",
    srcs = ["front_end_service.cc"],
    hdrs = ["front_end_service.h"],
    linkopts = [
        "-latomic",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":error_codes",
        ":front_end_utils",
        ":transaction_request_router",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/pbs/transactions/src:pbs_transactions_lib",
        "//cc/public/cpio/utils/metric_aggregation/interface:metric_aggregation_interface",
        "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
        "//cc/public/cpio/utils/metric_aggregation/src:metric_aggregation",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@io_opentelemetry_cpp//sdk/src/metrics",
        "@nlohmann_json//:lib",
    ],
)

cc_library(
    name = "front_end_service_v2",
    srcs = ["front_end_service_v2.cc"],
    hdrs = ["front_end_service_v2.h"],
    linkopts = [
        "-latomic",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":error_codes",
        ":front_end_utils",
        ":metric_initialization",
        ":transaction_request_router",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/pbs/budget_key_timeframe_manager/src:pbs_budget_key_timeframe_manager_lib",
        "//cc/pbs/consume_budget/src/gcp:consume_budget",
        "//cc/pbs/consume_budget/src/gcp:error_codes",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/pbs/transactions/src:pbs_transactions_lib",
        "//cc/public/cpio/utils/metric_aggregation/interface:metric_aggregation_interface",
        "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
        "//cc/public/cpio/utils/metric_aggregation/src:metric_aggregation",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@io_opentelemetry_cpp//sdk/src/metrics",
        "@nlohmann_json//:lib",
    ],
)
