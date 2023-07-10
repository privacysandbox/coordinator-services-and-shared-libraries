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
    name = "instance_client_provider_lib",
    srcs = select({
        "//cc/public/cpio/interface:aws_cpio_lib": ["//cc/cpio/client_providers/instance_client_provider_new/src/aws:aws_instance_client_provider_srcs"],
        "//cc/public/cpio/interface:gcp_cpio_lib": ["//cc/cpio/client_providers/instance_client_provider_new/src/gcp:gcp_instance_client_provider_srcs"],
        "//conditions:default": ["//cc/cpio/client_providers/instance_client_provider_new/src/aws:aws_instance_client_provider_srcs"],
    }) + ["//cc/cpio/client_providers/interface:instance_client_provider_interface.h"],
    copts = [
        "-std=c++17",
    ],
    deps = select({
        "//cc/public/cpio/interface:aws_cpio_lib": [
            "//cc/cpio/common/src/aws:aws_utils_lib",
            "//cc/core/async_executor/src/aws:core_aws_async_executor_lib",
            "@aws_sdk_cpp//:core",
            "@aws_sdk_cpp//:ec2",
        ],
        "//cc/public/cpio/interface:gcp_cpio_lib": [
            "@com_google_absl//absl/strings:str_format",
        ],
        "//conditions:default": [
            "//cc/cpio/common/src/aws:aws_utils_lib",
            "//cc/core/async_executor/src/aws:core_aws_async_executor_lib",
            "@aws_sdk_cpp//:core",
            "@aws_sdk_cpp//:ec2",
        ],
    }) + [
        "//cc:cc_base_include_dir",
        "//cc/core/common/uuid/src:uuid_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/cpio/client_providers/auth_token_provider/src:auth_token_provider_lib",
        "//cc/cpio/common/src:cpio_utils_lib",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/core/interface:interface_lib",
        "//cc/public/cpio/interface:cpio_errors",
        "@com_google_absl//absl/strings",
        "@nlohmann_json//:lib",
    ],
)
