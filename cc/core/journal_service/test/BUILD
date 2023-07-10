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

cc_test(
    name = "journal_service_tests",
    size = "small",
    srcs = glob(
        [
            "**/*.cc",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    linkopts = [
        "-latomic",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/blob_storage_provider/mock:blob_storage_provider_mock",
        "//cc/core/common/serialization/src:serialization_lib",
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/journal_service/mock:core_journal_service_mock",
        "//cc/core/journal_service/src:core_journal_service_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/metric_client_provider/mock:metric_client_provider_mock",
        "//cc/public/core/test/interface:execution_result_test_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
