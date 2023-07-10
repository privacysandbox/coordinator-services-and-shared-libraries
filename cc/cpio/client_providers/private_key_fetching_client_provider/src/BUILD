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
    name = "private_key_fetching_client_provider_lib",
    srcs = select({
        "//cc/public/cpio/interface:aws_cpio_lib": ["//cc/cpio/client_providers/private_key_fetching_client_provider/src/aws:aws_private_key_fetching_client_provider_srcs"],
        "//conditions:default": ["//cc/cpio/client_providers/private_key_fetching_client_provider/src/aws:aws_private_key_fetching_client_provider_srcs"],
    }) + [
        "private_key_fetching_client_provider_srcs",
        "//cc/cpio/client_providers/interface:private_key_fetching_client_provider_interface.h",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = select({
        "//cc/public/cpio/interface:aws_cpio_lib": [
            "@boost//:system",
            "@com_github_nghttp2_nghttp2//:nghttp2",
        ],
        "//conditions:default": [
            "@boost//:system",
            "@com_github_nghttp2_nghttp2//:nghttp2",
        ],
    }) + [
        "//cc:cc_base_include_dir",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/cpio/client_providers/global_cpio/src:global_cpio_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "@nlohmann_json//:lib",
    ],
)

exports_files([
    "private_key_fetching_client_provider.cc",
    "private_key_fetching_client_provider.h",
    "private_key_fetching_client_utils.cc",
    "private_key_fetching_client_utils.h",
    "error_codes.h",
])

filegroup(
    name = "private_key_fetching_client_provider_srcs",
    srcs = [
        ":error_codes.h",
        ":private_key_fetching_client_provider.cc",
        ":private_key_fetching_client_provider.h",
        ":private_key_fetching_client_utils.cc",
        ":private_key_fetching_client_utils.h",
    ],
)
