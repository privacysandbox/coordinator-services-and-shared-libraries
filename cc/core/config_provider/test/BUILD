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
    name = "config_provider_test",
    size = "small",
    srcs = ["config_provider_test.cc"],
    data = ["resources/test_config.json"],
    deps = [
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "env_config_provider_test",
    size = "small",
    srcs = ["env_config_provider_test.cc"],
    deps = [
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
