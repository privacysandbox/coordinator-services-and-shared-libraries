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
    name = "pbs_partition_request_router_test",
    size = "small",
    srcs = [
        "http_request_route_resolver_for_partition_test.cc",
        "transaction_request_router_for_partition_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/transaction_manager/mock:core_transaction_manager_mock",
        "//cc/pbs/partition/mock:pbs_partition_mock",
        "//cc/pbs/partition_manager/mock:pbs_partition_manager_mock",
        "//cc/pbs/partition_namespace/mock:pbs_partition_namespace_mock",
        "//cc/pbs/partition_request_router/src:pbs_partition_request_router_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
