# Copyright 2023 Google LLC
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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "configuration_fetcher",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/server/interface:server_interface_lib",
        "//cc/cpio/server/interface/crypto_service:crypto_service_interface_lib",
        "//cc/cpio/server/interface/job_service:job_service_interface_lib",
        "//cc/cpio/server/interface/nosql_database_service:nosql_database_service_interface_lib",
        "//cc/cpio/server/interface/queue_service:queue_service_interface_lib",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/interface/instance_client",
        "//cc/public/cpio/interface/parameter_client",
        "//cc/public/cpio/proto/crypto_service/v1:crypto_service_cc_proto",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
        "//cc/public/cpio/proto/parameter_service/v1:parameter_service_cc_proto",
        "//cc/public/cpio/utils/configuration_fetcher/interface:configuration_fetcher_interface",
        "//cc/public/cpio/utils/sync_utils",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)
