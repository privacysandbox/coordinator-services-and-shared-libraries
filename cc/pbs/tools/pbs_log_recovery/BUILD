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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "pbs_log_recovery_aws",
    srcs = ["pbs_log_recovery_on_aws.cc"],
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

cc_library(
    name = "read_journal_internal",
    srcs = ["read_journal_internal.cc"],
    hdrs = ["read_journal_internal.h"],
    visibility = ["//cc/pbs/tools/pbs_log_recovery:__subpackages__"],
    deps = [
        "//cc/core/blob_storage_provider/mock:blob_storage_provider_mock",
        "//cc/core/journal_service/interface:core_journal_service_interface_lib",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/journal_service/src/proto:core_journal_service_proto_lib",
        "//cc/core/transaction_manager/src/proto:core_transaction_manager_proto_lib",
        "//cc/pbs/budget_key_provider/src/proto:pbs_budget_key_provider_proto_lib",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:flags",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "read_journal_internal_test",
    srcs = ["read_journal_internal_test.cc"],
    linkopts = [
        "-latomic",
    ],
    deps = [
        ":read_journal_internal",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
