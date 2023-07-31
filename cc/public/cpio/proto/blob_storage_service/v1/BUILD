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
    name = "blob_storage_service_proto",
    srcs = ["blob_storage_service.proto"],
    deps = [
        "//cc/core/common/proto:core_common_proto",
    ],
)

##############################################################################
# C++
##############################################################################
cc_proto_library(
    name = "blob_storage_service_cc_proto",
    deps = [":blob_storage_service_proto"],
)

cc_grpc_library(
    name = "blob_storage_service_grpc",
    srcs = [":blob_storage_service_proto"],
    generate_mocks = True,
    grpc_only = True,
    deps = [":blob_storage_service_cc_proto"],
)
