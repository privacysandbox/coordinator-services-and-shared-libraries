load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "common_cloudfront_alarms_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/common/cloudfront_alarms",
    ],
    prefix = "monitoring/common/cloudfront_alarms",
    strip_prefix = ".",
)

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
    name = "common_api_gateway_alarms_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/common/api_gateway_alarms",
    ],
    prefix = "monitoring/common/api_gateway_alarms",
    strip_prefix = ".",
)

pkg_files(
    name = "common_heartbeat_prober_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/common/heartbeat_prober",
    ],
    prefix = "monitoring/common/heartbeat_prober",
    strip_prefix = ".",
)

pkg_files(
    name = "distributedpbs_heartbeat_prober_monitoring_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/distributedpbs_heartbeat_prober",
    ],
    prefix = "monitoring/distributedpbs_heartbeat_prober",
    strip_prefix = ".",
)

pkg_files(
    name = "mpkhs_publickey_heartbeat_prober_monitoring_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/mpkhs_publickey_heartbeat_prober",
    ],
    prefix = "monitoring/mpkhs_publickey_heartbeat_prober",
    strip_prefix = ".",
)

pkg_files(
    name = "mpkhs_privatekey_heartbeat_prober_monitoring_pkg",
    srcs = [
        "//coordinator/terraform/aws/monitoring/mpkhs_privatekey_heartbeat_prober",
    ],
    prefix = "monitoring/mpkhs_privatekey_heartbeat_prober",
    strip_prefix = ".",
)
