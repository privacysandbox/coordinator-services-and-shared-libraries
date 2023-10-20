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
    name = "gcp_queue_client_provider_lib",
    srcs = [
        ":gcp_queue_client_provider_srcs",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/instance_client_provider/src/gcp:gcp_instance_client_provider_lib",
        "//cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/cpio/common/src/gcp:gcp_utils_lib",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/proto/queue_service/v1:queue_service_cc_proto",
        "@com_github_googleapis_google_cloud_cpp//:pubsub",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googleapis//google/pubsub/v1:pubsub_cc_grpc",
        "@com_google_protobuf//:protobuf",
    ],
)

exports_files([
    "error_codes.h",
    "gcp_queue_client_provider.cc",
    "gcp_queue_client_provider.h",
])

filegroup(
    name = "gcp_queue_client_provider_srcs",
    srcs = [
        ":error_codes.h",
        ":gcp_queue_client_provider.cc",
        ":gcp_queue_client_provider.h",
    ],
)
