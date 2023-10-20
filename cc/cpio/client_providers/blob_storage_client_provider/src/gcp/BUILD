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
    name = "gcp_blob_storage_provider_lib",
    srcs = [":gcp_blob_storage_client_provider_srcs"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/common/time_provider/src:time_provider_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/blob_storage_client_provider/src/common:core_blob_storage_provider_common_lib",
        "//cc/cpio/client_providers/instance_client_provider/src/gcp:gcp_instance_client_provider_lib",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/common/src/gcp:gcp_utils_lib",
        "@com_github_googleapis_google_cloud_cpp//:storage",
    ],
)

exports_files([
    "gcp_cloud_storage_client_provider.h",
    "gcp_cloud_storage_client_provider.cc",
    "gcp_cloud_storage_utils.h",
])

filegroup(
    name = "gcp_blob_storage_client_provider_srcs",
    srcs = [
        ":gcp_cloud_storage_client_provider.cc",
        ":gcp_cloud_storage_client_provider.h",
        ":gcp_cloud_storage_utils.h",
    ],
)
