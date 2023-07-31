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

load(
    "@com_google_googleapis_imports//:imports.bzl",
    "cc_grpc_library",
    "cc_proto_library",
)

package(default_visibility = ["//cc:scp_internal_pkg"])

proto_library(
    name = "proto",
    srcs = glob(
        [
            "*.proto",
        ],
        exclude = [
            "cpio_service.proto",
        ],
    ),
    deps = [
        "//cc/core/common/proto:core_common_proto",
        "//cc/public/cpio/proto/private_key_service/v1:private_key_service_proto",
        "//cc/public/cpio/proto/public_key_service/v1:public_key_service_proto",
    ],
)

proto_library(
    name = "cpio_service_proto",
    srcs = ["cpio_service.proto"],
    deps = [
        "//cc/core/common/proto:core_common_proto",
        "@com_google_protobuf//:any_proto",
    ],
)

##############################################################################
# C++
##############################################################################
cc_proto_library(
    name = "cc_proto",
    deps = [":proto"],
)

cc_proto_library(
    name = "cpio_service_cc_proto",
    deps = [":cpio_service_proto"],
)

cc_grpc_library(
    name = "cpio_service_cc_grpc",
    srcs = [":cpio_service_proto"],
    generate_mocks = True,
    grpc_only = True,
    deps = [":cpio_service_cc_proto"],
)
