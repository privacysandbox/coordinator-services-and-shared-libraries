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
    name = "gcp_private_key_fetcher_provider_lib",
    srcs = [
        ":gcp_private_key_fetcher_provider_srcs",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/cpio/client_providers/auth_token_provider/src/gcp:gcp_auth_token_provider_lib",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/cpio/client_providers/private_key_fetcher_provider/src:private_key_fetcher_provider_lib",
        "//cc/public/cpio/interface/private_key_client:type_def",
    ],
)

exports_files([
    "error_codes.h",
    "gcp_private_key_fetcher_provider.cc",
    "gcp_private_key_fetcher_provider.h",
])

filegroup(
    name = "gcp_private_key_fetcher_provider_srcs",
    srcs = [
        ":error_codes.h",
        ":gcp_private_key_fetcher_provider.cc",
        ":gcp_private_key_fetcher_provider.h",
    ],
)
