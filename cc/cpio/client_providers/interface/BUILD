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

package(default_visibility = ["//cc:pbs_visibility"])

cc_library(
    name = "cpio_client_providers_interface_lib",
    srcs = [
        ":cpio_client_provider_interface_srcs",
    ],
    deps = [
        "//cc/core/interface:interface_lib",
        "//cc/public/core/interface:execution_result",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

exports_files([
    "instance_client_provider_interface.h",
    "auth_token_provider_interface.h",
])

filegroup(
    name = "cpio_client_provider_interface_srcs",
    srcs = [
        ":auth_token_provider_interface.h",
        ":instance_client_provider_interface.h",
    ],
)

cc_library(
    name = "type_def",
    srcs = [
        "type_def.h",
    ],
    deps = [
        "//cc/core/interface:interface_lib",
        "@com_google_protobuf//:protobuf",
    ],
)
