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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "s2n_tls",
    srcs = glob(
        [
            "lib/libs2n.a",
        ],
    ),
    hdrs = glob(
        [
            "include/s2n.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
    ],
)

cc_library(
    name = "aws_c_sdkutils",
    srcs = glob(
        [
            "lib/libaws-c-sdkutils.a",
        ],
    ),
    hdrs = glob(
        [
            "include/aws/sdkutils/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        "@aws_c_common",
    ],
)

cc_library(
    name = "aws_c_cal",
    srcs = glob(
        [
            "lib/libaws-c-cal.a",
        ],
    ),
    hdrs = glob(
        [
            "include/aws/cal/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        "@aws_c_common",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
    ],
)

cc_library(
    name = "aws_c_io",
    srcs = glob(
        [
            "lib/libaws-c-io.a",
        ],
    ),
    hdrs = glob(
        [
            "include/aws/io/*.h",
            "include/aws/io/private/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":aws_c_cal",
        ":s2n_tls",
        "@aws_c_common",
    ],
)

cc_library(
    name = "aws_c_compression",
    srcs = glob(
        [
            "lib/libaws-c-compression.a",
        ],
    ),
    hdrs = glob(
        [
            "include/aws/compression/*.h",
            "include/aws/compression/private/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        "@aws_c_common",
    ],
)

cc_library(
    name = "aws_c_http",
    srcs = glob(
        [
            "lib/libaws-c-http.a",
        ],
    ),
    hdrs = glob(
        [
            "include/aws/http/*.h",
            "include/aws/http/private/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":aws_c_cal",
        ":aws_c_compression",
        ":aws_c_io",
        "@aws_c_common",
    ],
)

cc_library(
    name = "aws_c_auth",
    srcs = glob(
        [
            "lib/libaws-c-auth.a",
        ],
    ),
    hdrs = glob(
        [
            "include/aws/auth/*.h",
            "include/aws/auth/external/*.h",
            "include/aws/auth/private/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":aws_c_compression",
        ":aws_c_http",
        ":aws_c_io",
        ":aws_c_sdkutils",
    ],
)

cc_library(
    name = "json_c",
    srcs = glob(
        [
            "lib/libjson-c.a",
        ],
    ),
    hdrs = glob(
        [
            "include/json-c/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
    ],
)

cc_library(
    name = "aws_nitro_enclaves_nsm_api",
    srcs = glob(
        [
            "lib/libnsm.so",
        ],
    ),
    hdrs = glob(
        [
            "include/nsm.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
    ],
)

cc_library(
    name = "aws_nitro_enclaves_sdk",
    srcs = glob(
        [
            "lib/libaws-nitro-enclaves-sdk-c.a",
        ],
    ),
    hdrs = glob(
        [
            "include/aws/nitro_enclaves/attestation.h",
            "include/aws/nitro_enclaves/exports.h",
            "include/aws/nitro_enclaves/kms.h",
            "include/aws/nitro_enclaves/nitro_enclaves.h",
            "include/aws/nitro_enclaves/rest.h",
            "include/aws/nitro_enclaves/internal/cms.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":aws_c_auth",
        ":aws_c_http",
        ":aws_c_io",
        ":aws_nitro_enclaves_nsm_api",
        ":json_c",
        "@aws_c_common",
    ],
)
