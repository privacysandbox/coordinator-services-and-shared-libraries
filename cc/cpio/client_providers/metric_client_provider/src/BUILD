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
    name = "metric_client_provider_select_lib",
    deps = select(
        {
            "//cc/public/cpio/interface:aws_cpio_lib": [
                "//cc/cpio/client_providers/metric_client_provider/src/aws:aws_metric_client_provider_lib",
            ],
            "//cc/public/cpio/interface:gcp_cpio_lib": [
                "//cc/cpio/client_providers/metric_client_provider/src/gcp:gcp_metric_client_provider_lib",
            ],
        },
        no_match_error = "Please build for AWS or GCP",
    ),
)

cc_library(
    name = "metric_client_provider_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/public/core/interface:execution_result",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface/metric_client:metric_client_interface",
        "//cc/public/cpio/interface/metric_client:type_def",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
        "@com_google_protobuf//:protobuf",
    ],
)
