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

cloud_platform_copts = select({
    "//cc:cloud_platform_gcp": [
        "-DPBS_GCP=1",
    ],
    "//cc:cloud_platform_local": [
        "-DPBS_LOCAL=1",
    ],
    "//conditions:default": [],
})

cc_library(
    name = "error_codes",
    hdrs = ["error_codes.h"],
    copts = cloud_platform_copts,
    deps = [
        "//cc/core/interface:errors_lib",
        "//cc/public/core/interface:execution_result",
    ],
)

cc_library(
    name = "pbs_instance_configuration",
    hdrs = ["pbs_instance_configuration.h"],
    copts = cloud_platform_copts,
    deps = [
        ":pbs_instance_logging",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/common/uuid/src:uuid_lib",
        "//cc/core/interface:interface_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/public/core/interface:execution_result",
    ],
)

cc_library(
    name = "pbs_instance_logging",
    hdrs = ["pbs_instance_logging.h"],
    copts = cloud_platform_copts,
    deps = [
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/common/uuid/src:uuid_lib",
        "//cc/public/core/interface:execution_result",
    ],
)

cc_library(
    name = "pbs_instance_v3",
    srcs = ["pbs_instance_v3.cc"],
    hdrs = ["pbs_instance_v3.h"],
    copts = cloud_platform_copts,
    deps = [
        ":error_codes",
        ":pbs_instance_configuration",
        ":pbs_instance_logging",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/authorization_proxy/src:core_authorization_proxy_lib",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/http2_server/src:core_http2_server_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/src/authentication:telemetry_authentication",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/pbs/front_end_service/src:front_end_service_v2",
        "//cc/pbs/health_service/src:pbs_health_service_lib",
        "//cc/pbs/interface:pbs_interface_lib",
    ],
)
