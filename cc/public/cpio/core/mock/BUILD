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
    name = "lib_cpio_mock",
    testonly = True,
    srcs = ["mock_lib_cpio.h"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/client_providers/global_cpio/mock:global_cpio_mock",
        "//cc/cpio/client_providers/global_cpio/src:global_cpio_lib",
    ],
)
