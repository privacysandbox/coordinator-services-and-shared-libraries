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

cc_test(
    name = "pbs_leasable_lock_test",
    size = "small",
    srcs = [
        "leasable_lock_on_nosql_database_helpers_test.cc",
        "leasable_lock_on_nosql_database_test.cc",
        "lease_info_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/lease_manager/mock:core_lease_manager_mock",
        "//cc/core/lease_manager/src:core_lease_manager_lib",
        "//cc/core/logger/interface:logger_interface_lib",
        "//cc/core/logger/mock:logger_mock",
        "//cc/core/logger/src:logger_lib",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/test:core_test_lib",
        "//cc/pbs/leasable_lock/mock:pbs_leasable_lock_mock",
        "//cc/pbs/leasable_lock/src:pbs_leasable_lock_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
