# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "configuration_fetcher_test",
    size = "small",
    srcs = ["configuration_fetcher_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/server/interface:server_interface_lib",
        "//cc/cpio/server/interface/job_service:job_service_interface_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/mock/instance_client:instance_client_mock",
        "//cc/public/cpio/mock/parameter_client:parameter_client_mock",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
        "//cc/public/cpio/proto/parameter_service/v1:parameter_service_cc_proto",
        "//cc/public/cpio/utils/configuration_fetcher/src:configuration_fetcher",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
