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
    name = "blob_storage_provider_mock",
    srcs = glob(
        [
            "**/*.h",
        ],
    ),
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/blob_storage_provider/src/aws:core_blob_storage_provider_aws_lib",
        "//cc/core/blob_storage_provider/src/common:core_blob_storage_provider_common_lib",
        "//cc/core/interface:interface_lib",
        "@com_google_absl//absl/strings",
    ],
)
