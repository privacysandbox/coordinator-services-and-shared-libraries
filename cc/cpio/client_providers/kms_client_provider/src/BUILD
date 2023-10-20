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
    name = "kms_client_provider_select_lib",
    deps =
        select(
            {
                "//cc/public/cpio/interface:aws_cpio_lib_inside_tee": [
                    "//cc/cpio/client_providers/kms_client_provider/src/aws:tee_aws_kms_client_provider_lib",
                ],
                "//cc/public/cpio/interface:aws_cpio_lib_outside_tee": [
                    "//cc/cpio/client_providers/kms_client_provider/src/aws:nontee_aws_kms_client_provider_lib",
                ],
                "//cc/public/cpio/interface:gcp_cpio_lib_inside_tee": [
                    "//cc/cpio/client_providers/kms_client_provider/src/gcp:tee_gcp_kms_client_provider_lib",
                ],
                "//cc/public/cpio/interface:gcp_cpio_lib_outside_tee": [
                    "//cc/cpio/client_providers/kms_client_provider/src/gcp:nontee_gcp_kms_client_provider_lib",
                ],
            },
            no_match_error = "Please build for AWS or GCP, inside TEE or outside TEE",
        ),
)
