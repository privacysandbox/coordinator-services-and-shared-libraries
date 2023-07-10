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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "type_def",
    hdrs =
        [
            "type_def.h",
        ],
    deps = [
        "//cc:cc_base_include_dir",
    ],
)

cc_library(
    name = "metric_client",
    srcs = ["//cc/public/cpio/adapters/metric_client/src:metric_client_srcs"],
    hdrs =
        [
            "metric_client_interface.h",
        ],
    deps = [
        ":type_def",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/cpio/proto:cc_proto",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
    ],
)

exports_files(["metric_client_interface.h"])
