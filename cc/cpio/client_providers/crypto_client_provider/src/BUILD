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
    name = "crypto_client_provider_lib",
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
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/crypto_client:type_def",
        "//cc/public/cpio/proto/crypto_service/v1:crypto_service_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@tink_cc//:binary_keyset_reader",
        "@tink_cc//:cleartext_keyset_handle",
        "@tink_cc//hybrid/internal:hpke_context",
        "@tink_cc//proto:hpke_cc_proto",
        "@tink_cc//subtle",
        "@tink_cc//util:secret_data",
    ],
)
