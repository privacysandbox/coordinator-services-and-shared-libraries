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
    name = "aws_nitro_enclaves_sdk",
    srcs = glob([
        "source/attestation.c",
        "source/cms.c",
        "source/kms.c",
        "source/nitro_enclaves.c",
        "source/rest.c",
    ]),
    hdrs = glob([
        "include/aws/nitro_enclaves/attestation.h",
        "include/aws/nitro_enclaves/exports.h",
        "include/aws/nitro_enclaves/kms.h",
        "include/aws/nitro_enclaves/nitro_enclaves.h",
        "include/aws/nitro_enclaves/rest.h",
        "include/aws/nitro_enclaves/internal/cms.h",
    ]),
    defines = [
    ],
    includes = [
        "include",
    ],
    deps = [
        "@aws_c_auth",
        "@aws_c_common",
        "@aws_c_http",
        "@aws_c_io",
        "@aws_nitro_enclaves_nsm_api",
        "@json_c",
    ],
)
