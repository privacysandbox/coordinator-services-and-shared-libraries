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

load("@rules_cc//cc:defs.bzl", "cc_binary")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "pbs_log_recovery_aws",
    srcs = glob(["*.cc"]),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/blob_storage_provider/src/aws:core_blob_storage_provider_aws_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//cc/core/transaction_manager/src:core_transaction_manager_lib",
        "//cc/pbs/budget_key_provider/src:pbs_budget_key_provider_lib",
        "//cc/pbs/transactions/src:pbs_transactions_lib",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "@aws_sdk_cpp//:core",
        "@aws_sdk_cpp//:s3",
    ],
)
