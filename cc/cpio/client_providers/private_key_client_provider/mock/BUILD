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
    name = "private_key_client_provider_mock",
    testonly = True,
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
        "//cc/core/http2_client/mock:http2_client_mock",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/kms_client_provider/mock:kms_client_provider_mock",
        "//cc/cpio/client_providers/private_key_client_provider/src:private_key_client_provider_lib",
        "//cc/cpio/client_providers/private_key_fetcher_provider/mock:private_key_fetcher_provider_mock",
        "@com_google_protobuf//:protobuf",
    ],
)
