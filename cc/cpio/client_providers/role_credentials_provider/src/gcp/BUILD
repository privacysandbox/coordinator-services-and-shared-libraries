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
    name = "gcp_role_credentials_provider_lib",
    srcs = [
        "//cc/cpio/client_providers/interface:role_credentials_provider_interface.h",
        "//cc/cpio/client_providers/role_credentials_provider/src/gcp:gcp_role_credentials_provider_srcs",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/instance_client_provider/src/gcp:gcp_instance_client_provider_lib",
        "//cc/public/cpio/interface:type_def",
    ],
)

exports_files([
    "gcp_role_credentials_provider.h",
    "gcp_role_credentials_provider.cc",
])

filegroup(
    name = "gcp_role_credentials_provider_srcs",
    srcs = [
        ":gcp_role_credentials_provider.cc",
        ":gcp_role_credentials_provider.h",
    ],
)
