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

cc_test(
    name = "journal_service_tests",
    size = "small",
    srcs = [
        "journal_input_stream_test.cc",
        "journal_output_stream_test.cc",
        "journal_service_serialization_test.cc",
        "journal_service_test.cc",
        "journal_utils_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    linkopts = [
        "-latomic",
    ],
    deps = [
        ":test_util",
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/blob_storage_provider/mock:blob_storage_provider_mock",
        "//cc/core/common/serialization/src:serialization_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_util",
    testonly = True,
    srcs = ["test_util.cc"],
    hdrs = ["test_util.h"],
    deps = [
        "//cc/core/blob_storage_provider/mock:blob_storage_provider_mock",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/journal_service/src/proto:core_journal_service_proto_lib",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "journal_input_stream_benchmark_test",
    size = "large",
    srcs = ["journal_input_stream_benchmark_test.cc"],
    copts = [
        "-std=c++17",
    ],
    linkopts = [
        "-latomic",
    ],
    tags = ["manual"],
    deps = [
        ":test_util",
        "//cc/core/blob_storage_provider/mock:blob_storage_provider_mock",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@google_benchmark//:benchmark",
    ],
)
