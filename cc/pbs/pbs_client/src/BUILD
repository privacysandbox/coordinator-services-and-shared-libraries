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
    name = "pbs_client_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/authorization_service/src:core_authorization_service",
        "//cc/core/common/operation_dispatcher/src:operation_dispatcher_lib",
        "//cc/core/credentials_provider/src:core_credentials_provider_lib",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/transaction_manager/mock:core_transaction_manager_mock",
        "//cc/core/transaction_manager/src:core_transaction_manager_lib",
        "//cc/pbs/front_end_service/src:front_end_service",
        "//cc/pbs/interface:pbs_interface_lib",
        "@nlohmann_json//:lib",
    ],
)
