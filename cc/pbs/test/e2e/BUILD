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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "pbs_e2e_test",
    size = "small",
    srcs = ["pbs_e2e_test.cc"],
    copts = [
        "-std=c++17",
    ],
    tags = ["manual"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/logger/src:logger_lib",
        "//cc/core/logger/src/log_providers/syslog:syslog_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/pbs_client/src:pbs_client_lib",
        "//cc/pbs/pbs_server/src/pbs_instance:pbs_instance_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

# To run this test locally, use command:
#   ./cc/tools/build/run_within_container.sh
#   --bazel_command="bazel test --//cc/pbs/pbs_server/src/pbs_instance:platform=aws_integration_test //cc/pbs/test/e2e:pbs_integration_test" --bazel_output_directory=/tmp/my_output_dir
cc_test(
    name = "pbs_integration_test",
    size = "large",
    timeout = "long",
    srcs = ["pbs_integration_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/credentials_provider/mock:core_credentials_provider_mock",
        "//cc/core/credentials_provider/src:core_credentials_provider_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/interface:logger_interface",
        "//cc/core/logger/src:logger_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/logger/src/log_providers/syslog:syslog_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/token_provider_cache/mock:core_token_provider_cache_mock",
        "//cc/pbs/pbs_client/src/transactional:pbs_transactional_client_lib",
        "//cc/pbs/test/e2e/test_pbs_server_starter:test_pbs_server_starter_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@aws_sdk_cpp//:core",
        "@com_google_googletest//:gtest_main",
    ],
)
