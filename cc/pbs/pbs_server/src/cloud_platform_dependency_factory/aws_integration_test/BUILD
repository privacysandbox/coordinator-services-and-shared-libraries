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

cc_library(
    name = "pbs_aws_integration_test_cloud_platform_dependency_factory_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/authorization_proxy/mock:core_authorization_proxy_mock",
        "//cc/core/authorization_proxy/src:core_authorization_proxy_lib",
        "//cc/core/blob_storage_provider/src/aws:core_blob_storage_provider_aws_lib",
        "//cc/core/blob_storage_provider/test/aws:test_aws_s3_lib",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/credentials_provider/mock:core_credentials_provider_mock",
        "//cc/core/credentials_provider/src:core_credentials_provider_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/nosql_database_provider/src/aws:core_nosql_database_provider_aws_lib",
        "//cc/core/nosql_database_provider/test/aws:test_aws_dynamo_db_lib",
        "//cc/core/token_provider_cache/mock:core_token_provider_cache_mock",
        "//cc/cpio/client_providers/instance_client_provider/test/aws:test_aws_instance_client_provider_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/cpio/client_providers/metric_client_provider/test/aws:test_aws_metric_client_provider_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/pbs/pbs_client/src:pbs_client_lib",
        "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/aws:pbs_aws_cloud_platform_dependency_factory_lib",
        "//cc/public/cpio/interface/metric_client",
        "//cc/public/cpio/utils/metric_aggregation/interface:metric_aggregation_interface",
        "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
    ],
)
