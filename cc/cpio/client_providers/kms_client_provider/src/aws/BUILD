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
    name = "nontee_aws_kms_client_provider_lib",
    srcs = [
        ":nontee_aws_kms_client_provider_srcs",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src/aws:core_aws_async_executor_lib",
        "//cc/core/common/uuid/src:uuid_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/common/src/aws:aws_utils_lib",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/kms_client:type_def",
        "@aws_sdk_cpp//:kms",
        "@tink_cc",
    ],
)

cc_library(
    name = "tee_aws_kms_client_provider_lib",
    srcs = [
        ":tee_aws_kms_client_provider_srcs",
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
        "//cc/cpio/common/src/aws:aws_utils_lib",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/kms_client:type_def",
        "@aws_sdk_cpp//:kms",
        "@tink_cc",
    ],
)

exports_files([
    "aws_kms_aead.h",
    "aws_kms_aead.cc",
    "nontee_aws_kms_client_provider.h",
    "nontee_aws_kms_client_provider.cc",
    "nontee_error_codes.h",
    "tee_aws_kms_client_provider.h",
    "tee_aws_kms_client_provider.cc",
    "tee_aws_kms_client_provider_utils.h",
    "tee_aws_kms_client_provider_utils.cc",
    "tee_error_codes.h",
])

filegroup(
    name = "nontee_aws_kms_client_provider_srcs",
    srcs = [
        ":aws_kms_aead.cc",
        ":aws_kms_aead.h",
        ":nontee_aws_kms_client_provider.cc",
        ":nontee_aws_kms_client_provider.h",
        ":nontee_error_codes.h",
    ],
)

filegroup(
    name = "tee_aws_kms_client_provider_srcs",
    srcs = [
        ":tee_aws_kms_client_provider.cc",
        ":tee_aws_kms_client_provider.h",
        ":tee_aws_kms_client_provider_utils.cc",
        ":tee_aws_kms_client_provider_utils.h",
        ":tee_error_codes.h",
    ],
)
