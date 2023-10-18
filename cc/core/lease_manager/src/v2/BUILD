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
    name = "core_lease_manager_v2_lib",
    srcs =
        [
            "component_lifecycle_lease_event_sink.cc",
            "component_lifecycle_lease_event_sink.h",
            "error_codes.h",
            "lease_manager_v2.cc",
            "lease_manager_v2.h",
            "lease_refresh_liveness_enforcer.cc",
            "lease_refresh_liveness_enforcer.h",
            "lease_refresher.cc",
            "lease_refresher.h",
            "lease_refresher_factory.cc",
            "lease_refresher_factory.h",
        ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/interface:interface_lib",
    ],
)
