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
    name = "aws_checksums",
    srcs =
        [
            "lib/libaws-checksums.a",
        ],
    hdrs = glob(
        [
            "include/aws/checksums/*.h",
        ],
    ),
    deps = [
        "@aws_c_common",
        "@curl",
    ],
)

cc_library(
    name = "aws_c_event_stream",
    srcs =
        [
            "lib/libaws-c-event-stream.a",
        ],
    hdrs = glob(
        [
            "include/aws/event-stream/*.h",
        ],
    ),
    deps = [
        ":aws_checksums",
        "@aws_c_common",
        "@curl",
    ],
)

cc_library(
    name = "core",
    srcs =
        [
            "lib/libaws-cpp-sdk-core.a",
        ],
    hdrs = glob(
        [
            "include/aws/core/*.h",
            "include/aws/core/**/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":aws_c_event_stream",
        ":aws_checksums",
        "@curl",
    ],
)

cc_library(
    name = "s3",
    srcs =
        [
            "lib/libaws-cpp-sdk-s3.a",
        ],
    hdrs = glob(
        [
            "include/aws/s3/*.h",
            "include/aws/s3/model/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":core",
        "@curl",
    ],
)

cc_library(
    name = "dynamodb",
    srcs =
        [
            "lib/libaws-cpp-sdk-dynamodb.a",
        ],
    hdrs = glob(
        [
            "include/aws/dynamodb/*.h",
            "include/aws/dynamodb/model/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":core",
        "@curl",
    ],
)

cc_library(
    name = "ec2",
    srcs =
        [
            "lib/libaws-cpp-sdk-ec2.a",
        ],
    hdrs = glob(
        [
            "include/aws/ec2/*.h",
            "include/aws/ec2/model/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":core",
        "@curl",
    ],
)

cc_library(
    name = "monitoring",
    srcs =
        [
            "lib/libaws-cpp-sdk-monitoring.a",
        ],
    hdrs = glob(
        [
            "include/aws/monitoring/*.h",
            "include/aws/monitoring/model/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":core",
        "@curl",
    ],
)

cc_library(
    name = "ssm",
    srcs =
        [
            "lib/libaws-cpp-sdk-ssm.a",
        ],
    hdrs = glob(
        [
            "include/aws/ssm/*.h",
            "include/aws/ssm/model/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":core",
        "@curl",
    ],
)

cc_library(
    name = "sts",
    srcs =
        [
            "lib/libaws-cpp-sdk-sts.a",
        ],
    hdrs = glob(
        [
            "include/aws/sts/*.h",
            "include/aws/sts/model/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":core",
        "@curl",
    ],
)

cc_library(
    name = "kms",
    srcs =
        [
            "lib/libaws-cpp-sdk-kms.a",
        ],
    hdrs = glob(
        [
            "include/aws/kms/*.h",
            "include/aws/kms/model/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":core",
        "@curl",
    ],
)

cc_library(
    name = "sqs",
    srcs =
        [
            "lib/libaws-cpp-sdk-sqs.a",
        ],
    hdrs = glob(
        [
            "include/aws/sqs/*.h",
            "include/aws/sqs/model/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":core",
        "@curl",
    ],
)

cc_library(
    name = "autoscaling",
    srcs =
        [
            "lib/libaws-cpp-sdk-autoscaling.a",
        ],
    hdrs = glob(
        [
            "include/aws/autoscaling/*.h",
            "include/aws/autoscaling/model/*.h",
        ],
    ),
    includes = [
        "include",
    ],
    deps = [
        ":core",
        "@curl",
    ],
)
