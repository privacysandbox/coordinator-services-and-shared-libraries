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

load("@rules_cc//cc:defs.bzl", "cc_binary")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "privacy_budget_service",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts =
        [
            "-std=c++17",
        ],
    linkopts = [
        # Tell LD to look in the working directory of the executable for SO
        # files to link
        "-Wl,-rpath,'$$ORIGIN'",
    ],
    malloc = "@jemalloc//:libjemalloc_static",
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/logger/src:logger_lib",
        "//cc/core/logger/src/log_providers/syslog:syslog_lib",
        "//cc/pbs/pbs_server/src/pbs_instance:pbs_instance_lib",
        "//cc/pbs/pbs_server/src/pbs_instance:pbs_instance_multi_partition_lib",
        "@com_google_absl//absl/debugging:failure_signal_handler",
    ],
)
