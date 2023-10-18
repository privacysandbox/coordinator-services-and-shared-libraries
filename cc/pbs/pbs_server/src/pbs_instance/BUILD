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

load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cloud_platform_copts = select({
    "cloud_platform_aws": [
        "-DPBS_AWS=1",
    ],
    "cloud_platform_aws_integration_test": [
        "-DPBS_AWS_INTEGRATION_TEST=1",
    ],
    "cloud_platform_gcp": [
        "-DPBS_GCP=1",
    ],
    "cloud_platform_gcp_integration_test": [
        "-DPBS_GCP_INTEGRATION_TEST=1",
    ],
    "cloud_platform_local": [
        "-DPBS_LOCAL=1",
    ],
    "//conditions:default": [],
})

deps = [
    "//cc:cc_base_include_dir",
    "//cc/core/async_executor/src:core_async_executor_lib",
    "//cc/core/authorization_proxy/src:core_authorization_proxy_lib",
    "//cc/core/blob_storage_provider/src/aws:core_blob_storage_provider_aws_lib",
    "//cc/core/config_provider/src:config_provider_lib",
    "//cc/core/credentials_provider/src:core_credentials_provider_lib",
    "//cc/core/curl_client/src:http1_curl_client_lib",
    "//cc/core/http2_server/src:core_http2_server_lib",
    "//cc/core/interface:interface_lib",
    "//cc/core/journal_service/src:core_journal_service_lib",
    "//cc/core/lease_manager/src:core_lease_manager_lib",
    "//cc/core/lease_manager/src/v2:core_lease_manager_v2_lib",
    "//cc/pbs/partition_lease_event_sink/src:pbs_partition_lease_event_sink_lib",
    "//cc/core/tcp_traffic_forwarder/src:core_tcp_traffic_forwarder",
    "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
    "//cc/core/transaction_manager/interface:core_transaction_manager_interface_lib",
    "//cc/core/transaction_manager/src:core_transaction_manager_lib",
    "//cc/public/cpio/utils/metric_aggregation/interface:metric_aggregation_interface",
    "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
    "//cc/pbs/budget_key_provider/src:pbs_budget_key_provider_lib",
    "//cc/pbs/checkpoint_service/src:pbs_checkpoint_service_lib",
    "//cc/pbs/front_end_service/src:pbs_front_end_service_lib",
    "//cc/pbs/health_service/src:pbs_health_service_lib",
    "//cc/pbs/leasable_lock/src:pbs_leasable_lock_lib",
    "//cc/pbs/remote_transaction_manager/src:pbs_remote_transaction_manager_lib",
    "//cc/public/cpio/interface/metric_client",
    "@com_google_absl//absl/strings",
    "//cc/core/http2_forwarder/src:core_http2_forwarder_lib",
    "//cc/pbs/partition_namespace/src:pbs_partition_namespace_lib",
    "//cc/pbs/partition_request_router/src:pbs_partition_request_router_lib",
    "//cc/pbs/partition_lease_preference_applier/src:pbs_partition_lease_preference_applier_lib",
]

aws_deps = deps + [
    "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/aws:pbs_aws_cloud_platform_dependency_factory_lib",
]

aws_integration_test_deps = deps + [
    "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/aws_integration_test:pbs_aws_integration_test_cloud_platform_dependency_factory_lib",
]

gcp_deps = deps + [
    "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp:pbs_gcp_cloud_platform_dependency_factory_lib",
]

gcp_integration_test_deps = deps + [
    "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp_integration_test:pbs_gcp_integration_test_cloud_platform_dependency_factory_lib",
]

local_deps = deps + [
    "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/local:pbs_local_cloud_platform_dependency_factory_lib",
]

cc_library(
    name = "pbs_instance_lib",
    srcs = glob(
        [
            "pbs_instance.cc",
            "pbs_instance.h",
            "error_codes.h",
            "pbs_instance_configuration.h",
            "pbs_instance_logging.h",
        ],
    ),
    copts = cloud_platform_copts + [
        "-std=c++17",
    ],
    deps = select({
        "cloud_platform_aws": aws_deps,
        "cloud_platform_aws_integration_test": aws_integration_test_deps,
        "cloud_platform_gcp": gcp_deps,
        "cloud_platform_gcp_integration_test": gcp_integration_test_deps,
        "cloud_platform_local": local_deps,
        "//conditions:default": [],
    }),
)

cc_library(
    name = "pbs_instance_v2_lib",
    srcs = glob(
        [
            "pbs_instance_v2.cc",
            "pbs_instance_v2.h",
            "error_codes.h",
            "pbs_instance_configuration.h",
            "pbs_instance_logging.h",
        ],
    ),
    copts = cloud_platform_copts + [
        "-std=c++17",
    ],
    deps = select({
        "cloud_platform_aws": aws_deps,
        "cloud_platform_aws_integration_test": aws_integration_test_deps,
        "cloud_platform_gcp": gcp_deps,
        "cloud_platform_gcp_integration_test": gcp_integration_test_deps,
        "cloud_platform_local": local_deps,
        "//conditions:default": [],
    }),
)

cc_library(
    name = "pbs_instance_multi_partition_lib",
    srcs = glob(
        [
            "pbs_instance_multi_partition.cc",
            "pbs_instance_multi_partition.h",
            "synchronous_executor.h",
            "pbs_instance_multi_partition_platform_wrapper.cc",
            "pbs_instance_multi_partition_platform_wrapper.h",
            "error_codes.h",
            "pbs_instance_configuration.h",
            "pbs_instance_logging.h",
        ],
    ),
    copts = cloud_platform_copts + [
        "-std=c++17",
    ],
    deps = select({
        "cloud_platform_aws": aws_deps,
        "cloud_platform_aws_integration_test": aws_integration_test_deps,
        "cloud_platform_gcp": gcp_deps,
        "cloud_platform_gcp_integration_test": gcp_integration_test_deps,
        "cloud_platform_local": local_deps,
        "//conditions:default": [],
    }),
)

string_flag(
    name = "platform",
    build_setting_default = "local",
    values = [
        "aws",
        "aws_integration_test",
        "gcp",
        "gcp_integration_test",
        "local",
    ],
)

config_setting(
    name = "cloud_platform_aws",
    flag_values = {"platform": "aws"},
)

config_setting(
    name = "cloud_platform_aws_integration_test",
    flag_values = {"platform": "aws_integration_test"},
)

config_setting(
    name = "cloud_platform_gcp",
    flag_values = {"platform": "gcp"},
)

config_setting(
    name = "cloud_platform_gcp_integration_test",
    flag_values = {"platform": "gcp_integration_test"},
)

config_setting(
    name = "cloud_platform_local",
    flag_values = {"platform": "local"},
)
