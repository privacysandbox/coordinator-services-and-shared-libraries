package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "common",
    srcs =
        [
            "lib/libaws-c-common.a",
        ],
    hdrs = glob(
        [
            "include/aws/common/*.h",
        ],
    ),
    deps = [
        "@curl",
    ],
)

cc_library(
    name = "checksums",
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
        ":common",
        "@curl",
    ],
)

cc_library(
    name = "event_stream",
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
        ":checksums",
        ":common",
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
    strip_include_prefix = "include",
    deps = [
        ":checksums",
        ":event_stream",
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
    strip_include_prefix = "include",
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
    strip_include_prefix = "include",
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
    strip_include_prefix = "include",
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
    strip_include_prefix = "include",
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
    strip_include_prefix = "include",
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
    strip_include_prefix = "include",
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
    strip_include_prefix = "include",
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
    strip_include_prefix = "include",
    deps = [
        ":core",
        "@curl",
    ],
)
