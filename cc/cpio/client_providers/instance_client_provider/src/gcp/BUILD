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

exports_files([
    "gcp_instance_client_provider.h",
    "gcp_instance_client_provider.cc",
    "gcp_instance_client_utils.cc",
    "gcp_instance_client_utils.h",
    "error_codes.h",
])

filegroup(
    name = "gcp_instance_client_provider_srcs",
    srcs = [
        ":error_codes.h",
        ":gcp_instance_client_provider.cc",
        ":gcp_instance_client_provider.h",
        ":gcp_instance_client_utils.cc",
        ":gcp_instance_client_utils.h",
    ],
)

cc_library(
    name = "gcp_instance_client_provider_lib",
    srcs = [
        "error_codes.h",
        "gcp_instance_client_provider.cc",
        "gcp_instance_client_provider.h",
        "gcp_instance_client_utils.cc",
        "gcp_instance_client_utils.h",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/common/src:cpio_utils_lib",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@nlohmann_json//:lib",
    ],
)
