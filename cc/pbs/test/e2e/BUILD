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

package(default_visibility = ["//visibility:private"])

cc_test(
    name = "pbs_gcp_integration_test",
    size = "large",
    timeout = "long",
    srcs = ["pbs_gcp_integration_test.cc"],
    data = [
        "//cc/pbs/deploy/pbs_server/build_defs:pbs_cloud_run_container_for_local.tar",
    ],
    tags = ["manual"],
    deps = [
        "//cc/core/common/operation_dispatcher/src:operation_dispatcher_lib",
        "//cc/core/http2_client/src:synchronous_http2_client",
        "//cc/core/test/utils/docker_helper:docker_helper_lib",
        "//cc/pbs/proto/storage:budget_value_cc_proto",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//proto/pbs/api/v1:api_cc_proto",
        "@com_github_googleapis_google_cloud_cpp//:spanner",
        "@com_github_googleapis_google_cloud_cpp//:storage",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "postsubmit_gcp_integration_test",
    size = "small",
    srcs = ["postsubmit_gcp_integration_test.cc"],
    tags = ["manual"],
    deps = [
        "//cc/core/common/operation_dispatcher/src:operation_dispatcher_lib",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/http2_client/src:synchronous_http2_client",
        "//cc/core/test/utils:utils_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//proto/pbs/api/v1:api_cc_proto",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
