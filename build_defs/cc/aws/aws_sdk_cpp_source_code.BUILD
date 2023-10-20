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
    name = "core",
    srcs = glob([
        "aws-cpp-sdk-core/source/*.cpp",
        "aws-cpp-sdk-core/source/external/tinyxml2/*.cpp",
        "aws-cpp-sdk-core/source/external/cjson/*.cpp",
        "aws-cpp-sdk-core/source/auth/*.cpp",
        "aws-cpp-sdk-core/source/client/*.cpp",
        "aws-cpp-sdk-core/source/internal/*.cpp",
        "aws-cpp-sdk-core/source/aws/model/*.cpp",
        "aws-cpp-sdk-core/source/http/*.cpp",
        "aws-cpp-sdk-core/source/http/standard/*.cpp",
        "aws-cpp-sdk-core/source/config/*.cpp",
        "aws-cpp-sdk-core/source/monitoring/*.cpp",
        "aws-cpp-sdk-core/source/utils/*.cpp",
        "aws-cpp-sdk-core/source/utils/event/*.cpp",
        "aws-cpp-sdk-core/source/utils/base64/*.cpp",
        "aws-cpp-sdk-core/source/utils/crypto/*.cpp",
        "aws-cpp-sdk-core/source/utils/json/*.cpp",
        "aws-cpp-sdk-core/source/utils/threading/*.cpp",
        "aws-cpp-sdk-core/source/utils/xml/*.cpp",
        "aws-cpp-sdk-core/source/utils/logging/*.cpp",
        "aws-cpp-sdk-core/source/utils/memory/*.cpp",
        "aws-cpp-sdk-core/source/utils/memory/stl/*.cpp",
        "aws-cpp-sdk-core/source/utils/stream/*.cpp",
        "aws-cpp-sdk-core/source/utils/crypto/factory/*.cpp",
        "aws-cpp-sdk-core/source/http/curl/*.cpp",
        "aws-cpp-sdk-core/source/utils/crypto/openssl/*.cpp",
        "aws-cpp-sdk-core/source/net/linux-shared/*.cpp",  # NET_SOURCE
        "aws-cpp-sdk-core/source/platform/linux-shared/*.cpp",  # PLATFORM_LINUX_SHARED_SOURCE
    ]),
    hdrs = [
        "aws-cpp-sdk-core/include/aws/core/SDKConfig.h",
    ] + glob([
        "aws-cpp-sdk-core/include/aws/core/*.h",
        "aws-cpp-sdk-core/include/aws/core/auth/*.h",
        "aws-cpp-sdk-core/include/aws/core/client/*.h",
        "aws-cpp-sdk-core/include/aws/core/internal/*.h",
        "aws-cpp-sdk-core/include/aws/core/net/*.h",
        "aws-cpp-sdk-core/include/aws/core/http/*.h",
        "aws-cpp-sdk-core/include/aws/core/http/standard/*.h",
        "aws-cpp-sdk-core/include/aws/core/config/*.h",
        "aws-cpp-sdk-core/include/aws/core/monitoring/*.h",
        "aws-cpp-sdk-core/include/aws/core/platform/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/event/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/base64/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/crypto/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/json/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/threading/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/xml/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/memory/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/memory/stl/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/logging/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/ratelimiter/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/stream/*.h",
        "aws-cpp-sdk-core/include/aws/core/external/cjson/*.h",
        "aws-cpp-sdk-core/include/aws/core/external/tinyxml2/*.h",
        "aws-cpp-sdk-core/include/aws/core/http/curl/*.h",
        "aws-cpp-sdk-core/include/aws/core/utils/crypto/openssl/*.h",
    ]),
    defines = [
        'AWS_SDK_VERSION_STRING=\\"1.8.186\\"',
        "AWS_SDK_VERSION_MAJOR=1",
        "AWS_SDK_VERSION_MINOR=8",
        "AWS_SDK_VERSION_PATCH=186",
        "ENABLE_OPENSSL_ENCRYPTION=1",
        "ENABLE_CURL_CLIENT=1",
        "OPENSSL_IS_BORINGSSL=1",
        "PLATFORM_LINUX",
    ],
    includes = [
        "aws-cpp-sdk-core/include",
    ],
    linkopts = ["-ldl"],
    deps = [
        "@aws_c_event_stream",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@curl",
    ],
)

cc_library(
    name = "monitoring",
    srcs = glob([
        "aws-cpp-sdk-monitoring/include/**/*.h",
        "aws-cpp-sdk-monitoring/source/**/*.cpp",
    ]),
    hdrs = glob([
        "aws-cpp-sdk-monitoring/include/aws/monitoring/*.h",
        "aws-cpp-sdk-monitoring/include/aws/monitoring/model/*.h",
    ]),
    includes = [
        "aws-cpp-sdk-monitoring/include/",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "ec2",
    srcs = glob([
        "aws-cpp-sdk-ec2/include/**/*.h",
        "aws-cpp-sdk-ec2/source/**/*.cpp",
    ]),
    hdrs = glob([
        "aws-cpp-sdk-ec2/include/aws/ec2/*.h",
        "aws-cpp-sdk-ec2/include/aws/ec2/model/*.h",
    ]),
    includes = [
        "aws-cpp-sdk-ec2/include/",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "sts",
    srcs = glob([
        "aws-cpp-sdk-sts/include/**/*.h",
        "aws-cpp-sdk-sts/source/**/*.cpp",
    ]),
    hdrs = glob([
        "aws-cpp-sdk-sts/include/aws/sts/*.h",
        "aws-cpp-sdk-sts/include/aws/sts/model/*.h",
    ]),
    includes = [
        "aws-cpp-sdk-sts/include/",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "ssm",
    srcs = glob([
        "aws-cpp-sdk-ssm/include/**/*.h",
        "aws-cpp-sdk-ssm/source/**/*.cpp",
    ]),
    hdrs = glob([
        "aws-cpp-sdk-ssm/include/aws/ssm/*.h",
        "aws-cpp-sdk-ssm/include/aws/ssm/model/*.h",
    ]),
    includes = [
        "aws-cpp-sdk-ssm/include/",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "s3",
    srcs = glob([
        "aws-cpp-sdk-s3/source/*.cpp",  # AWS_S3_SOURCE
        "aws-cpp-sdk-s3/source/model/*.cpp",  # AWS_S3_MODEL_SOURCE
    ]),
    hdrs = glob([
        "aws-cpp-sdk-s3/include/aws/s3/*.h",  # AWS_S3_HEADERS
        "aws-cpp-sdk-s3/include/aws/s3/model/*.h",  # AWS_S3_MODEL_HEADERS
    ]),
    includes = [
        "aws-cpp-sdk-s3/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "dynamodb",
    srcs = glob([
        "aws-cpp-sdk-dynamodb/source/*.cpp",  # SOURCE
        "aws-cpp-sdk-dynamodb/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "aws-cpp-sdk-dynamodb/include/aws/dynamodb/*.h",  # HEADERS
        "aws-cpp-sdk-dynamodb/include/aws/dynamodb/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "aws-cpp-sdk-dynamodb/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "dynamodbstreams",
    srcs = glob([
        "aws-cpp-sdk-dynamodbstreams/source/*.cpp",  # SOURCE
        "aws-cpp-sdk-dynamodbstreams/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "aws-cpp-sdk-dynamodbstreams/include/aws/dynamodbstreams/*.h",  # HEADERS
        "aws-cpp-sdk-dynamodbstreams/include/aws/dynamodbstreams/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "aws-cpp-sdk-dynamodbstreams/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "kms",
    srcs = glob([
        "aws-cpp-sdk-kms/source/*.cpp",  # SOURCE
        "aws-cpp-sdk-kms/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "aws-cpp-sdk-kms/include/aws/kms/*.h",  # HEADERS
        "aws-cpp-sdk-kms/include/aws/kms/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "aws-cpp-sdk-kms/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "sqs",
    srcs = glob([
        "aws-cpp-sdk-sqs/source/*.cpp",  # SOURCE
        "aws-cpp-sdk-sqs/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "aws-cpp-sdk-sqs/include/aws/sqs/*.h",  # HEADERS
        "aws-cpp-sdk-sqs/include/aws/sqs/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "aws-cpp-sdk-sqs/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "autoscaling",
    srcs = glob([
        "aws-cpp-sdk-autoscaling/source/*.cpp",  # SOURCE
        "aws-cpp-sdk-autoscaling/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "aws-cpp-sdk-autoscaling/include/aws/autoscaling/*.h",  # HEADERS
        "aws-cpp-sdk-autoscaling/include/aws/autoscaling/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "aws-cpp-sdk-autoscaling/include",
    ],
    deps = [
        ":core",
    ],
)

genrule(
    name = "SDKConfig_h",
    outs = [
        "aws-cpp-sdk-core/include/aws/core/SDKConfig.h",
    ],
    cmd_bash = "touch '$@'",
)
