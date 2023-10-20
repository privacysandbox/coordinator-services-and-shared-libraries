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
    name = "private_key_client_provider_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/cpio/client_providers/kms_client_provider/src:kms_client_provider_select_lib",
        "//cc/cpio/client_providers/private_key_fetcher_provider/src:private_key_fetcher_provider_select_lib",
        "//cc/cpio/client_providers/role_credentials_provider/src:role_credentials_provider_select_lib",
        "//cc/public/cpio/interface/private_key_client:type_def",
        "//cc/public/cpio/proto/private_key_service/v1:private_key_service_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
