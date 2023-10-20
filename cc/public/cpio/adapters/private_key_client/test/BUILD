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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "private_key_client_test",
    size = "small",
    timeout = "long",
    srcs = ["private_key_client_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/private_key_client_provider/src:private_key_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/adapters/private_key_client/mock:private_key_client_mock",
        "//cc/public/cpio/core/mock:lib_cpio_mock",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/interface/private_key_client",
        "//cc/public/cpio/interface/private_key_client:type_def",
        "//cc/public/cpio/proto/private_key_service/v1:private_key_service_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)
