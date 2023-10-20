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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "nontee_aws_kms_client_provider_test",
    size = "small",
    timeout = "long",
    srcs =
        ["nontee_aws_kms_client_provider_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/kms_client_provider/mock/aws:aws_kms_client_provider_mock",
        "//cc/cpio/client_providers/kms_client_provider/src/aws:nontee_aws_kms_client_provider_lib",
        "//cc/cpio/client_providers/role_credentials_provider/mock:role_credentials_provider_mock",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tee_aws_kms_client_provider_test",
    size = "small",
    timeout = "long",
    srcs =
        ["tee_aws_kms_client_provider_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/kms_client_provider/mock/aws:aws_kms_client_provider_mock",
        "//cc/cpio/client_providers/kms_client_provider/src/aws:tee_aws_kms_client_provider_lib",
        "//cc/cpio/client_providers/role_credentials_provider/mock:role_credentials_provider_mock",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "aws_kms_aead_test",
    size = "small",
    timeout = "long",
    srcs =
        ["aws_kms_aead_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/kms_client_provider/mock/aws:aws_kms_client_provider_mock",
        "//cc/cpio/client_providers/kms_client_provider/src/aws:nontee_aws_kms_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tee_aws_kms_client_provider_utils_test",
    size = "small",
    srcs =
        ["tee_aws_kms_client_provider_utils_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/client_providers/kms_client_provider/src/aws:tee_aws_kms_client_provider_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_aws_kms_client_provider_lib",
    srcs = [
        "test_aws_kms_client_provider.cc",
        "test_aws_kms_client_provider.h",
        "//cc/cpio/client_providers/interface:kms_client_provider_interface.h",
        "//cc/cpio/client_providers/kms_client_provider/src/aws:nontee_aws_kms_client_provider_srcs",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//cc/cpio/client_providers/kms_client_provider/src/aws:nontee_aws_kms_client_provider_lib",
        "//cc/cpio/common/test/aws:test_aws_utils_lib",
        "//cc/public/cpio/test/kms_client:test_aws_kms_client_options",
    ],
)
