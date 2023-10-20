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

load("@rules_cc//cc:defs.bzl", "cc_binary")

package(default_visibility = ["//cc:scp_internal_pkg"])

LINK_OPTS = [
    # Tell LD to look in the working directory of the executable for SO
    # files to link
    "-Wl,-rpath,'$$ORIGIN'",
]

cc_binary(
    name = "blob_storage_client_test",
    srcs = ["blob_storage_client_test.cc"],
    copts = select({
        "//cc/public/cpio/interface:gcp_cpio_lib": [
            "-DGCP_CPIO=1",
        ],
        "//conditions:default": [
        ],
    }),
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/global_cpio/src:global_cpio_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/blob_storage_client",
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
    ],
)

cc_binary(
    name = "public_key_client_test",
    srcs = ["public_key_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/public_key_client",
    ],
)

cc_binary(
    name = "kms_client_test",
    srcs = ["kms_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/kms_client",
    ],
)

cc_binary(
    name = "local_private_key_client_test",
    testonly = True,
    srcs = ["local_private_key_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface/private_key_client",
        "//cc/public/cpio/test/global_cpio:test_lib_cpio",
    ],
)

cc_binary(
    name = "private_key_client_test",
    srcs = ["private_key_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/private_key_client",
    ],
)

cc_binary(
    name = "crypto_client_test",
    srcs = ["crypto_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/crypto_client",
    ],
)

cc_binary(
    name = "parameter_client_test",
    srcs = ["parameter_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/parameter_client",
        "//cc/public/cpio/proto/parameter_service/v1:parameter_service_cc_proto",
    ],
)

cc_binary(
    name = "local_parameter_client_test",
    srcs = ["local_parameter_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface/parameter_client",
        "//cc/public/cpio/proto/parameter_service/v1:parameter_service_cc_proto",
        "//cc/public/cpio/test/global_cpio:test_lib_cpio",
    ],
)

cc_binary(
    name = "instance_client_test",
    srcs = ["instance_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface:cpio",
        "//cc/public/cpio/interface/instance_client",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
    ],
)

cc_binary(
    name = "local_instance_client_test",
    srcs = ["local_instance_client_test.cc"],
    linkopts = LINK_OPTS,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/interface:errors",
        "//cc/public/cpio/interface/instance_client",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
        "//cc/public/cpio/test/global_cpio:test_lib_cpio",
    ],
)
