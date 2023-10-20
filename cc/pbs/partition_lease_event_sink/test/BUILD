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
    name = "pbs_partition_lease_event_sink_test",
    size = "small",
    srcs = [
        "partition_lease_event_sink_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/lease_manager/mock:core_lease_manager_mock",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/partition/mock:pbs_partition_mock",
        "//cc/pbs/partition_lease_event_sink/src:pbs_partition_lease_event_sink_lib",
        "//cc/pbs/partition_manager/mock:pbs_partition_manager_mock",
        "//cc/pbs/partition_manager/src:pbs_partition_manager_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_google_googletest//:gtest_main",
    ],
)
