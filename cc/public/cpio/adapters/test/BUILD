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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "public_mocks_test",
    srcs =
        [
            "public_mocks_test.cc",
        ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/public/cpio/mock/blob_storage_client:blob_storage_client_mock",
        "//cc/public/cpio/mock/crypto_client:crypto_client_mock",
        "//cc/public/cpio/mock/instance_client:instance_client_mock",
        "//cc/public/cpio/mock/kms_client:kms_client_mock",
        "//cc/public/cpio/mock/metric_client:metric_client_mock",
        "//cc/public/cpio/mock/parameter_client:parameter_client_mock",
        "//cc/public/cpio/mock/private_key_client:private_key_client_mock",
        "//cc/public/cpio/mock/public_key_client:public_key_client_mock",
        "//cc/public/cpio/utils/configuration_fetcher/mock:configuration_fetcher_mock",
        "@com_google_googletest//:gtest",
    ],
)
