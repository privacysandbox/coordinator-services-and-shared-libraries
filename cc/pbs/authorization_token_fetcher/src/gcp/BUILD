# Copyright 2023 Google LLC
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
    name = "pbs_gcp_authorization_token_fetcher",
    srcs = glob(
        [
            "**/*.h",
            "**/*.cc",
        ],
    ),
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/authorization_service/src:core_authorization_service",
        "//cc/core/credentials_provider/src:core_credentials_provider_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "@com_google_absl//absl/strings",
        "@curl",
        "@nlohmann_json//:lib",
    ],
)
