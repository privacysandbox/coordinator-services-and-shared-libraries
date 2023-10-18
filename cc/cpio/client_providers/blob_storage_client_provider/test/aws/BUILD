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

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "aws_s3_test",
    size = "small",
    srcs = [
        "aws_s3_client_provider_stream_test.cc",
        "aws_s3_client_provider_test.cc",
        "aws_s3_utils_test.cc",
        "mock_s3_client.h",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src/aws:core_aws_async_executor_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/cpio/client_providers/blob_storage_client_provider/src/aws:aws_blob_storage_client_provider_lib",
        "//cc/cpio/client_providers/instance_client_provider/mock:instance_client_provider_mock",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@aws_sdk_cpp//:core",
        "@aws_sdk_cpp//:s3",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_aws_blob_storage_client_provider_lib",
    srcs = [
        "test_aws_blob_storage_client_provider.cc",
        "test_aws_blob_storage_client_provider.h",
        "//cc/cpio/client_providers/blob_storage_client_provider/src/aws:aws_blob_storage_client_provider_srcs",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//cc/cpio/client_providers/blob_storage_client_provider/src/aws:aws_blob_storage_client_provider_lib",
        "//cc/cpio/common/test/aws:test_aws_utils_lib",
        "//cc/public/cpio/test/blob_storage_client:test_aws_blob_storage_client_options",
    ],
)
