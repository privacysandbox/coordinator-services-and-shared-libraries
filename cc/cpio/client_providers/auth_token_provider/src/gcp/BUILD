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
    name = "gcp_auth_token_provider_lib",
    srcs = [
        "//cc/cpio/client_providers/auth_token_provider/src/gcp:gcp_auth_token_provider_srcs",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface:type_def",
        "@com_google_absl//absl/strings",
        "@nlohmann_json//:lib",
    ],
)

exports_files([
    "gcp_auth_token_provider.h",
    "gcp_auth_token_provider.cc",
    "error_codes.h",
])

filegroup(
    name = "gcp_auth_token_provider_srcs",
    srcs = [
        ":error_codes.h",
        ":gcp_auth_token_provider.cc",
        ":gcp_auth_token_provider.h",
    ],
)
