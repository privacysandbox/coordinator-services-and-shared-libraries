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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "gcp_queue_client_provider_test",
    size = "small",
    srcs = [
        "gcp_queue_client_provider_test.cc",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/instance_client_provider/mock:instance_client_provider_mock",
        "//cc/cpio/client_providers/queue_client_provider/mock/gcp:gcp_queue_client_provider_mock",
        "//cc/cpio/client_providers/queue_client_provider/src/gcp:gcp_queue_client_provider_lib",
        "//cc/cpio/common/src/gcp:gcp_utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/proto/queue_service/v1:queue_service_cc_proto",
        "@com_google_googleapis//google/pubsub/v1:pubsub_cc_grpc",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "test_gcp_queue_client_provider_lib",
    srcs = [
        "test_gcp_queue_client_provider.cc",
        "test_gcp_queue_client_provider.h",
        "//cc/cpio/client_providers/interface:queue_client_provider_interface.h",
        "//cc/cpio/client_providers/queue_client_provider/src/gcp:gcp_queue_client_provider_srcs",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/client_providers/queue_client_provider/src/gcp:gcp_queue_client_provider_lib",
    ],
)
