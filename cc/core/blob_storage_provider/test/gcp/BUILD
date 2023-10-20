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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "gcp_cloud_storage_test",
    size = "small",
    srcs = [
        "gcp_cloud_storage_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/blob_storage_provider/mock:blob_storage_provider_mock",
        "//cc/core/blob_storage_provider/src/gcp:core_blob_storage_provider_gcp_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/lease_manager/src:core_lease_manager_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/leasable_lock/src:pbs_leasable_lock_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//third_party/cloud_cpp/google/cloud/storage/testing:mock_client",
        "@com_github_googleapis_google_cloud_cpp//:storage",
        "@com_google_googletest//:gtest_main",
    ],
)

# We declare a library using the same specs as we would a test so that we can
# make sure it compiles as a presubmit, otherwise it is skipped.
cc_library(
    name = "gcp_cloud_storage_async_test_library",
    testonly = True,
    srcs = ["gcp_cloud_storage_async_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/blob_storage_provider/mock:blob_storage_provider_mock",
        "//cc/core/blob_storage_provider/src/gcp:core_blob_storage_provider_gcp_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/lease_manager/src:core_lease_manager_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/leasable_lock/src:pbs_leasable_lock_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_googleapis_google_cloud_cpp//:storage",
        "@com_google_googletest//:gtest_main",
    ],
)

# This manual test runs against a prod instance of Cloud Storage
# The bucket is automatically created and destroyed within the provided
# project. Make sure the bucket name does not clash with any bucket
# you care about.
#
# To run against a prod instance of Cloud Storage, you need to use a service
# account. To do so, create a key for the account and export the variable path
# to it with `export GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json`
# The key can be created by following:
# https://cloud.google.com/iam/docs/creating-managing-service-account-keys#creating
#
# This test should be called via similar:
# `cc_test "cc/core/blob_storage_provider/test/gcp:gcp_cloud_storage_async_test \
#   --test_env GOOGLE_APPLICATION_CREDENTIALS=$GOOGLE_APPLICATION_CREDENTIALS"`
cc_test(
    name = "gcp_cloud_storage_async_test",
    timeout = "long",
    copts = [
        "-std=c++17",
    ],
    tags = ["manual"],
    deps = [
        ":gcp_cloud_storage_async_test_library",
    ],
)
