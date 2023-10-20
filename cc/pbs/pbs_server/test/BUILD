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
    name = "pbs_instance_configuration_test",
    size = "small",
    srcs = [
        "pbs_instance_configuration_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/pbs/pbs_server/src/pbs_instance:pbs_instance_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "pbs_instance_test",
    size = "small",
    srcs = [
        "pbs_instance_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/tcp_traffic_forwarder/mock:tcp_traffic_forwarder_mock",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/token_provider_cache/mock:core_token_provider_cache_mock",
        "//cc/pbs/leasable_lock/mock:pbs_leasable_lock_mock",
        "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/local:pbs_local_cloud_platform_dependency_factory_lib",
        "//cc/pbs/pbs_server/src/pbs_instance:pbs_instance_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "pbs_instance_v2_test",
    size = "small",
    srcs = [
        "pbs_instance_v2_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/tcp_traffic_forwarder/mock:tcp_traffic_forwarder_mock",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/token_provider_cache/mock:core_token_provider_cache_mock",
        "//cc/pbs/leasable_lock/mock:pbs_leasable_lock_mock",
        "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/local:pbs_local_cloud_platform_dependency_factory_lib",
        "//cc/pbs/pbs_server/src/pbs_instance:pbs_instance_v2_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
    ],
)

cc_test(
    name = "pbs_instance_multi_partition_test",
    size = "large",
    timeout = "long",
    srcs = [
        "pbs_instance_multi_partition_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    tags = ["manual"],
    deps = [
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/tcp_traffic_forwarder/mock:tcp_traffic_forwarder_mock",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/token_provider_cache/mock:core_token_provider_cache_mock",
        "//cc/pbs/leasable_lock/mock:pbs_leasable_lock_mock",
        "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/local:pbs_local_cloud_platform_dependency_factory_lib",
        "//cc/pbs/pbs_server/src/pbs_instance:pbs_instance_multi_partition_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
