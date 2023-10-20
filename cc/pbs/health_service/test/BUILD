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
    name = "health_service_test",
    size = "small",
    srcs = ["health_service_test.cc"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "files/five_percent_meminfo_file.txt",
        "files/invalid_format_meminfo_file.txt",
        "files/missing_available_meminfo_file.txt",
        "files/missing_total_meminfo_file.txt",
        "files/ninety_six_percent_meminfo_file.txt",
    ],
    deps = [
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/http2_server/mock:core_http2_server_mock",
        "//cc/pbs/health_service/src:pbs_health_service_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@com_google_googletest//:gtest_main",
    ],
)
