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

package(default_visibility = ["//visibility:private"])

cc_test(
    name = "pbs_gcp_cloud_dependency_factory_test",
    size = "small",
    srcs = [
        "gcp_cloud_dependency_factory_test.cc",
    ],
    deps = [
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/http2_client/mock:http2_client_mock",
        "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp:pbs_gcp_cloud_platform_dependency_factory_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
