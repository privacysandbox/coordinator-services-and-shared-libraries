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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "gcp_cloud_storage_client_provider_test",
    size = "small",
    srcs = [
        "gcp_cloud_storage_client_provider_test.cc",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/blob_storage_client_provider/src/gcp:gcp_blob_storage_provider_lib",
        "//cc/cpio/client_providers/instance_client_provider/mock:instance_client_provider_mock",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_googleapis_google_cloud_cpp//:storage",
        "@com_github_googleapis_google_cloud_cpp//google/cloud/storage:storage_client_testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "gcp_cloud_storage_client_provider_stream_test",
    size = "small",
    srcs = [
        "gcp_cloud_storage_client_provider_stream_test.cc",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/blob_storage_client_provider/src/gcp:gcp_blob_storage_provider_lib",
        "//cc/cpio/client_providers/instance_client_provider/mock:instance_client_provider_mock",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_googleapis_google_cloud_cpp//:storage",
        "@com_github_googleapis_google_cloud_cpp//google/cloud/storage:storage_client_testing",
        "@com_google_googletest//:gtest_main",
    ],
)
