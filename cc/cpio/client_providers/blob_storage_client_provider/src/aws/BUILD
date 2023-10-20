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
    name = "aws_blob_storage_client_provider_lib",
    srcs = [":aws_blob_storage_client_provider_srcs"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src/aws:core_aws_async_executor_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/blob_storage_client_provider/src/common:core_blob_storage_provider_common_lib",
        "//cc/cpio/client_providers/instance_client_provider/src/aws:aws_instance_client_provider_lib",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/common/src/aws:aws_utils_lib",
        "//cc/public/cpio/interface/blob_storage_client:type_def",
        "@aws_sdk_cpp//:core",
        "@aws_sdk_cpp//:s3",
        "@com_google_absl//absl/strings",
    ],
)

exports_files([
    "aws_s3_client_provider.h",
    "aws_s3_client_provider.cc",
    "aws_s3_utils.h",
])

filegroup(
    name = "aws_blob_storage_client_provider_srcs",
    srcs = [
        ":aws_s3_client_provider.cc",
        ":aws_s3_client_provider.h",
        ":aws_s3_utils.h",
    ],
)
