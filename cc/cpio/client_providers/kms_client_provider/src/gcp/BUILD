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
    name = "nontee_gcp_kms_client_provider_lib",
    srcs = [
        ":nontee_gcp_kms_client_provider_srcs",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/common/uuid/src:uuid_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/client_providers/kms_client_provider/interface/gcp:gcp_kms_client_provider_interface",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/kms_client:type_def",
        "@com_github_googleapis_google_cloud_cpp//:kms",
        "@com_google_absl//absl/strings",
        "@nlohmann_json//:lib",
        "@tink_cc",
    ],
)

cc_library(
    name = "tee_gcp_kms_client_provider_lib",
    srcs = [
        ":tee_gcp_kms_client_provider_srcs",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/common/uuid/src:uuid_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/client_providers/kms_client_provider/interface/gcp:gcp_kms_client_provider_interface",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/kms_client:type_def",
        "@com_github_googleapis_google_cloud_cpp//:kms",
        "@com_google_absl//absl/strings",
        "@nlohmann_json//:lib",
        "@tink_cc",
    ],
)

exports_files([
    "error_codes.h",
    "gcp_key_management_service_client.cc",
    "gcp_key_management_service_client.h",
    "gcp_kms_aead.cc",
    "gcp_kms_aead.h",
    "gcp_kms_client_provider.cc",
    "gcp_kms_client_provider.h",
    "nontee_gcp_kms_provider.cc",
    "tee_gcp_kms_client_provider_utils.cc",
    "tee_gcp_kms_client_provider_utils.h",
    "tee_gcp_kms_provider.cc",
])

filegroup(
    name = "gcp_kms_client_provider_srcs",
    srcs = [
        ":error_codes.h",
        ":gcp_key_management_service_client.cc",
        ":gcp_key_management_service_client.h",
        ":gcp_kms_aead.cc",
        ":gcp_kms_aead.h",
        ":gcp_kms_client_provider.cc",
        ":gcp_kms_client_provider.h",
    ],
)

filegroup(
    name = "nontee_gcp_kms_client_provider_srcs",
    srcs = [
        ":gcp_kms_client_provider_srcs",
        ":nontee_gcp_kms_aead_provider.cc",
    ],
)

filegroup(
    name = "tee_gcp_kms_client_provider_srcs",
    srcs = [
        ":gcp_kms_client_provider_srcs",
        ":tee_gcp_kms_aead_provider.cc",
        ":tee_gcp_kms_client_provider_utils.cc",
        ":tee_gcp_kms_client_provider_utils.h",
    ],
)
