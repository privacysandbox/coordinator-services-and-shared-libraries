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

package(default_visibility = ["//cc:pbs_visibility"])

cc_library(
    name = "pbs_local_cloud_platform_dependency_factory_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    deps = [
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/authorization_service/src:core_authorization_service",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/pbs/consume_budget/src/gcp:consume_budget",
        "//cc/pbs/interface:pbs_interface_lib",
        "@io_opentelemetry_cpp//sdk/src/resource",
    ],
)
