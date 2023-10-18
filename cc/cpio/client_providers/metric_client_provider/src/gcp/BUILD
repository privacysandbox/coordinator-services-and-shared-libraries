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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_library(
    name = "gcp_metric_client_provider_lib",
    srcs = [
        ":gcp_metric_client_provider_srcs",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/instance_client_provider/src/gcp:gcp_instance_client_provider_lib",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/cpio/common/src/gcp:gcp_utils_lib",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/metric_client:type_def",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "@com_github_googleapis_google_cloud_cpp//:monitoring",
        "@com_google_protobuf//:protobuf",
    ],
)

exports_files([
    "gcp_metric_client_provider.h",
    "gcp_metric_client_provider.cc",
    "gcp_metric_client_utils.h",
    "gcp_metric_client_utils.cc",
    "error_codes.h",
])

filegroup(
    name = "gcp_metric_client_provider_srcs",
    srcs = [
        ":error_codes.h",
        ":gcp_metric_client_provider.cc",
        ":gcp_metric_client_provider.h",
        ":gcp_metric_client_utils.cc",
        ":gcp_metric_client_utils.h",
    ],
)
