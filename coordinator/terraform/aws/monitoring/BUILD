# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
