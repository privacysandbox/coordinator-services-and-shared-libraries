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

package(default_visibility = ["//cc:scp_internal_pkg"])

LINK_OPTS = [
    # Tell LD to look in the working directory of the executable for SO
    # files to link
    "-Wl,-rpath,'$$ORIGIN'",
]

cc_binary(
    name = "config_client_test",
    srcs = ["config_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/config_client",
        "@aws_sdk_cpp//:core",
    ],
)

cc_binary(
    name = "metric_client_test",
    srcs = ["metric_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/metric_client",
        "@aws_sdk_cpp//:core",
    ],
)

cc_binary(
    name = "local_config_client_test",
    srcs = ["local_config_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface/config_client",
        "//cc/public/cpio/test/global_cpio:test_lib_cpio",
        "@aws_sdk_cpp//:core",
    ],
)

cc_binary(
    name = "local_metric_client_test",
    srcs = ["local_metric_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface/metric_client",
        "//cc/public/cpio/test/global_cpio:test_lib_cpio",
        "@aws_sdk_cpp//:core",
    ],
)

cc_binary(
    name = "public_key_client_test",
    srcs = ["public_key_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/public_key_client",
        "@aws_sdk_cpp//:core",
    ],
)

cc_binary(
    name = "local_private_key_client_test",
    srcs = ["local_private_key_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/test/global_cpio:test_lib_cpio",
        "//cc/public/cpio/test/private_key_client:test_private_key_client",
        "@aws_sdk_cpp//:core",
    ],
)

cc_binary(
    name = "private_key_client_test",
    srcs = ["private_key_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:async_context_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/private_key_client",
        "@aws_sdk_cpp//:core",
    ],
)
