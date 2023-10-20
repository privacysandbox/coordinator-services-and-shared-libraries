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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_library(
    name = "cpio_client_providers_interface_lib",
    srcs = [
        ":cpio_client_provider_interface_srcs",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/public/core/interface:execution_result",
        "//cc/public/cpio/interface:type_def",
        "//cc/public/cpio/interface/blob_storage_client:type_def",
        "//cc/public/cpio/interface/job_client:type_def",
        "//cc/public/cpio/interface/kms_client:type_def",
        "//cc/public/cpio/interface/metric_client:metric_client_interface",
        "//cc/public/cpio/interface/metric_client:type_def",
        "//cc/public/cpio/interface/parameter_client:type_def",
        "//cc/public/cpio/interface/private_key_client:type_def",
        "//cc/public/cpio/proto/blob_storage_service/v1:blob_storage_service_cc_proto",
        "//cc/public/cpio/proto/crypto_service/v1:crypto_service_cc_proto",
        "//cc/public/cpio/proto/instance_service/v1:instance_service_cc_proto",
        "//cc/public/cpio/proto/job_service/v1:job_service_cc_proto",
        "//cc/public/cpio/proto/kms_service/v1:kms_service_cc_proto",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "//cc/public/cpio/proto/nosql_database_service/v1:nosql_database_service_cc_proto",
        "//cc/public/cpio/proto/parameter_service/v1:parameter_service_cc_proto",
        "//cc/public/cpio/proto/private_key_service/v1:private_key_service_cc_proto",
        "//cc/public/cpio/proto/public_key_service/v1:public_key_service_cc_proto",
        "//cc/public/cpio/proto/queue_service/v1:queue_service_cc_proto",
        "//cc/public/cpio/utils/metric_aggregation/interface:type_def",
        "@com_google_protobuf//:protobuf",
    ],
)

exports_files([
    "auto_scaling_client_provider_interface.h",
    "blob_storage_client_provider_interface.h",
    "cpio_provider_interface.h",
    "crypto_client_provider_interface.h",
    "cloud_initializer_interface.h",
    "instance_client_provider_interface.h",
    "job_client_provider_interface.h",
    "kms_client_provider_interface.h",
    "metric_client_provider_interface.h",
    "nosql_database_client_provider_interface.h",
    "parameter_client_provider_interface.h",
    "auth_token_provider_interface.h",
    "private_key_client_provider_interface.h",
    "private_key_fetcher_provider_interface.h",
    "public_key_client_provider_interface.h",
    "queue_client_provider_interface.h",
    "role_credentials_provider_interface.h",
])

filegroup(
    name = "cpio_client_provider_interface_srcs",
    srcs = [
        ":auth_token_provider_interface.h",
        ":auto_scaling_client_provider_interface.h",
        ":blob_storage_client_provider_interface.h",
        ":cloud_initializer_interface.h",
        ":cpio_provider_interface.h",
        ":crypto_client_provider_interface.h",
        ":instance_client_provider_interface.h",
        ":job_client_provider_interface.h",
        ":kms_client_provider_interface.h",
        ":metric_client_provider_interface.h",
        ":nosql_database_client_provider_interface.h",
        ":parameter_client_provider_interface.h",
        ":private_key_client_provider_interface.h",
        ":private_key_fetcher_provider_interface.h",
        ":public_key_client_provider_interface.h",
        ":queue_client_provider_interface.h",
        ":role_credentials_provider_interface.h",
    ],
)

cc_library(
    name = "type_def",
    srcs =
        [
            "type_def.h",
        ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "@com_google_protobuf//:protobuf",
    ],
)
