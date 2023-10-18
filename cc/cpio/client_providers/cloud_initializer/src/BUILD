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
    name = "cloud_initializer_select_lib",
    deps = select(
        {
            "//cc/public/cpio/interface:aws_cpio_lib": [
                "//cc/cpio/client_providers/cloud_initializer/src/aws:aws_initializer_lib",
            ],
            "//cc/public/cpio/interface:gcp_cpio_lib": [
                "//cc/cpio/client_providers/cloud_initializer/src/gcp:no_op_initializer_lib",
            ],
        },
        no_match_error = "Please build for AWS or GCP",
    ),
)
