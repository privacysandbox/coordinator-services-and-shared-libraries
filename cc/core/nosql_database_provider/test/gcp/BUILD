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

package(default_visibility = ["//cc:scp_internal_pkg"])

TEST_SRCS = [
    "gcp_spanner_utils_test.cc",
    "gcp_spanner_test.cc",
]

cc_test(
    name = "gcp_spanner_test",
    size = "large",
    srcs =
        TEST_SRCS,
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/interface:logger_interface",
        "//cc/core/lease_manager/src:core_lease_manager_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/nosql_database_provider/src/gcp:core_nosql_database_provider_gcp_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/leasable_lock/src:pbs_leasable_lock_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_googleapis_google_cloud_cpp//:spanner",
        "@com_github_googleapis_google_cloud_cpp//:spanner_mocks",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:lib",
    ],
)

# We declare a library using the same specs as we would a test so that we can
# make sure it compiles as a presubmit, otherwise it is skipped.
cc_library(
    name = "gcp_spanner_async_test_library",
    testonly = True,
    srcs = ["gcp_spanner_async_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/interface:logger_interface",
        "//cc/core/lease_manager/src:core_lease_manager_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/nosql_database_provider/src/gcp:core_nosql_database_provider_gcp_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/leasable_lock/src:pbs_leasable_lock_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_googleapis_google_cloud_cpp//:spanner",
        "@com_github_googleapis_google_cloud_cpp//:spanner_mocks",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:lib",
    ],
)

# This manual test runs against a local instance of Spanner created using the
# emulator: https://cloud.google.com/spanner/docs/emulator#install
#
# The emulator should have a Project and Instance matching that configured in
# the test. The database is automatically created and destroyed within this
# project/instance so make sure the database name does not clash with any DB
# you care about.
#
# Activate the emulator config: `gcloud config configurations activate [EMULATOR]`
# Set the project: `gcloud config set project [MY-PROJECT]`
# Create the instance: `gcloud spanner instances create [MY-INSTANCE] \
#   --config=[EMULATOR] --description="Test Instance" --nodes=1`
#
# After starting the emulator, this test should be called via similar:
# `cc_test "cc/core/nosql_database_provider/test/gcp:gcp_spanner_async_test \
#   --test_env SPANNER_EMULATOR_HOST=localhost:9010"`
#
# If the test fails saying that the DB already exists, you can delete it manually
# with `gcloud spanner databases delete [MY-DATABASE] --instance=[MY-INSTANCE]`
#
# To run against production Spanner, create a project and instance and change
# the corresponding variables in the test. Then download a key for the service
# account to run as:
# https://cloud.google.com/iam/docs/creating-managing-service-account-keys#creating
# Export the variable: `export GOOGLE_APPLICATION_CREDENTIALS=/path/to/key`
# Then run the test with "--test_env GOOGLE_APPLICATION_CREDENTIALS=$GOOGLE_APPLICATION_CREDENTIALS
cc_test(
    name = "gcp_spanner_async_test",
    size = "large",
    copts = [
        "-std=c++17",
    ],
    tags = ["manual"],
    deps = [
        ":gcp_spanner_async_test_library",
    ],
)
