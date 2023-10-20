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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "core_journal_service_lib",
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
        "//cc/core/blob_storage_provider/src/common:core_blob_storage_provider_common_lib",
        "//cc/core/common/concurrent_queue/src:concurrent_queue_lib",
        "//cc/core/common/operation_dispatcher/src:operation_dispatcher_lib",
        "//cc/core/common/serialization/src:serialization_lib",
        "//cc/core/common/sized_or_timed_bytes_buffer/src:bytes_buffer_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/interface:core_journal_service_interface_lib",
        "//cc/core/journal_service/src/proto:core_journal_service_proto_lib",
        "//cc/core/logger/src:logger_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/public/cpio/utils/metric_aggregation/interface:metric_aggregation_interface",
        "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
        "//cc/public/cpio/utils/metric_aggregation/src:metric_aggregation",
        "@com_google_absl//absl/strings",
    ],
)
