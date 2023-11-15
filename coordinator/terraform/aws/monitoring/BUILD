load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "common_sqs_queue_alarms_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/common/sqs_queue_alarms",
    ],
    prefix = "monitoring/common/sqs_queue_alarms",
    strip_prefix = ".",
)

pkg_files(
    name = "common_lambda_alarms_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/common/lambda_alarms",
    ],
    prefix = "monitoring/common/lambda_alarms",
    strip_prefix = ".",
)

pkg_files(
    name = "common_mp_api_gateway_alarms_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/common/mp_api_gateway_alarms",
    ],
    prefix = "monitoring/common/mp_api_gateway_alarms",
    strip_prefix = ".",
)

pkg_files(
    name = "common_api_gateway_alarms_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/common/api_gateway_alarms",
    ],
    prefix = "monitoring/common/api_gateway_alarms",
    strip_prefix = ".",
)
