# Copyright 2024 Google LLC
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

load("@rules_cc//cc:defs.bzl", "cc_binary")

package(default_visibility = ["//cc:pbs_visibility"])

cc_library(
    name = "error_codes",
    hdrs = ["error_codes.h"],
    deps = [
        "//cc/core/interface:errors_lib",
        "//cc/public/core/interface:execution_result",
    ],
)

cloud_platform_copts = select({
    "//cc:cloud_platform_gcp": [
        "-DPBS_GCP=1",
    ],
    "//cc:cloud_platform_local": [
        "-DPBS_LOCAL=1",
    ],
    "//conditions:default": [],
})

cc_binary(
    name = "privacy_budget_service",
    srcs = ["pbs_server.cc"],
    copts = cloud_platform_copts,
    linkopts = [
        # Tell LD to look in the working directory of the executable for SO
        # files to link
        "-Wl,-rpath,'$$ORIGIN'",
    ],
    malloc = "@jemalloc//:libjemalloc_static",
    deps = select({
        "//cc:cloud_platform_gcp": ["//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp:pbs_gcp_cloud_platform_dependency_factory_lib"],
        "//cc:cloud_platform_local": [
            "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/local:pbs_local_cloud_platform_dependency_factory_lib",
        ],
        "//conditions:default": [],
    }) + [
        ":error_codes",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/logger/src:logger_lib",
        "//cc/core/logger/src/log_providers/stdout:stdout_lib",
        "//cc/core/logger/src/log_providers/syslog:syslog_lib",
        "//cc/pbs/pbs_server/src/pbs_instance:pbs_instance_v3",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/log:check",
    ],
)
