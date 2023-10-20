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

load("@rules_cc//cc:defs.bzl", "cc_proto_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//cc:scp_internal_pkg"])

proto_library(
    name = "hello_world_proto",
    srcs = ["hello_world.proto"],
    deps = [
        "//cc/core/common/proto:core_common_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "hello_world_cc_proto",
    deps = [":hello_world_proto"],
)

cc_test(
    name = "job_client_provider_test",
    size = "small",
    srcs = [
        "job_client_provider_test.cc",
        "job_client_utils_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        ":hello_world_cc_proto",
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/job_client_provider/mock:job_client_provider_mock",
        "//cc/cpio/client_providers/job_client_provider/src:job_client_provider_lib",
        "//cc/cpio/client_providers/nosql_database_client_provider/mock:mock_nosql_database_client_provider_lib",
        "//cc/cpio/client_providers/nosql_database_client_provider/src/common:nosql_database_provider_common_lib",
        "//cc/cpio/client_providers/queue_client_provider/mock:mock_queue_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/proto/job_service/v1:job_service_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
