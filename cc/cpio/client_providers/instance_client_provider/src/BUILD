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
        "//cc/public/cpio/interface:aws_cpio_lib": ["//cc/cpio/client_providers/instance_client_provider/src/aws:aws_instance_client_provider_srcs"],
        "//conditions:default": ["//cc/cpio/client_providers/instance_client_provider/src/aws:aws_instance_client_provider_srcs"],
    }) + ["//cc/cpio/client_providers/interface:instance_client_provider_interface.h"],
    copts = [
        "-std=c++17",
    ],
    deps = select({
        "//cc/public/cpio/interface:aws_cpio_lib": [
            "@aws_sdk_cpp//:core",
            "//cc/cpio/common/src/aws:aws_utils_lib",
            "@aws_sdk_cpp//:ec2",
        ],
        "//conditions:default": [
            "@aws_sdk_cpp//:core",
            "//cc/cpio/common/src/aws:aws_utils_lib",
            "@aws_sdk_cpp//:ec2",
        ],
    }) + [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src/aws:core_aws_async_executor_lib",
        "//cc/core/common/uuid/src:uuid_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/public/cpio/interface:cpio_errors",
    ],
)
