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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_library(
    name = "metric_client_provider_lib",
    srcs = select({
        "//cc/public/cpio/interface:aws_cpio_lib": ["//cc/cpio/client_providers/metric_client_provider/src/aws:aws_metric_client_provider_srcs"],
        "//cc/public/cpio/interface:gcp_cpio_lib": ["//cc/cpio/client_providers/metric_client_provider/src/gcp:gcp_metric_client_provider_srcs"],
        "//conditions:default": ["//cc/cpio/client_providers/metric_client_provider/src/aws:aws_metric_client_provider_srcs"],
    }) + [
        "//cc/cpio/client_providers/interface:metric_client_provider_interface.h",
        ":metric_client_provider_srcs",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = select({
        "//cc/public/cpio/interface:aws_cpio_lib": [
            "@aws_sdk_cpp//:monitoring",
            "//cc/cpio/common/src/aws:aws_utils_lib",
            "//cc/cpio/client_providers/instance_client_provider_new/src/aws:aws_instance_client_provider_lib",
        ],
        "//cc/public/cpio/interface:gcp_cpio_lib": [
            "//cc/cpio/cloud_providers/gcp/interface:cpio_gcp_interface_lib",
            "//cc/cpio/client_providers/instance_client_provider_new/src/gcp:gcp_instance_client_provider_lib",
            "@com_github_googleapis_google_cloud_cpp//:monitoring",
        ],
        "//conditions:default": [
            "@aws_sdk_cpp//:monitoring",
            "//cc/cpio/common/src/aws:aws_utils_lib",
            "//cc/cpio/client_providers/instance_client_provider_new/src/aws:aws_instance_client_provider_lib",
        ],
    }) + [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/global_cpio/src:global_cpio_lib",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/cpio/proto:cc_proto",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/core/interface:execution_result",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/metric_client:type_def",
        "@com_google_protobuf//:protobuf",
    ],
)

exports_files([
    "metric_client_provider.h",
    "metric_client_provider.cc",
    "error_codes.h",
    "metric_client_utils.h",
    "metric_client_utils.cc",
])

filegroup(
    name = "metric_client_provider_srcs",
    srcs = [
        ":error_codes.h",
        ":metric_client_provider.cc",
        ":metric_client_provider.h",
        ":metric_client_utils.cc",
        ":metric_client_utils.h",
    ],
)
