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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "auth_token_provider_lib",
    srcs = select({
        "//cc/public/cpio/interface:aws_cpio_lib": ["//cc/cpio/client_providers/auth_token_provider/src/aws:aws_auth_token_provider_srcs"],
        "//cc/public/cpio/interface:gcp_cpio_lib": ["//cc/cpio/client_providers/auth_token_provider/src/gcp:gcp_auth_token_provider_srcs"],
        "//conditions:default": ["//cc/cpio/client_providers/auth_token_provider/src/aws:aws_auth_token_provider_srcs"],
    }),
    copts = [
        "-std=c++17",
    ],
    deps = select({
        "//cc/public/cpio/interface:aws_cpio_lib": [
        ],
        "//cc/public/cpio/interface:gcp_cpio_lib": [
            "@com_google_absl//absl/strings",
            "@nlohmann_json//:lib",
        ],
        "//conditions:default": [
        ],
    }) + [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface:type_def",
    ],
)
