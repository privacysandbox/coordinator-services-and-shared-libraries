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
    name = "public_key_client_provider_lib",
    srcs = [
        ":public_key_client_provider_srcs",
        "//cc/cpio/client_providers/interface:public_key_client_provider_interface.h",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/client_providers/global_cpio/src:global_cpio_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/public/cpio/interface/public_key_client:type_def",
        "//cc/public/cpio/proto/public_key_service/v1:public_key_service_cc_proto",
        "@nlohmann_json//:lib",
    ],
)

exports_files([
    "error_codes.h",
    "public_key_client_provider.cc",
    "public_key_client_provider.h",
    "public_key_client_utils.cc",
    "public_key_client_utils.h",
])

filegroup(
    name = "public_key_client_provider_srcs",
    srcs = [
        "error_codes.h",
        "public_key_client_provider.cc",
        "public_key_client_provider.h",
        "public_key_client_utils.cc",
        "public_key_client_utils.h",
    ],
)
