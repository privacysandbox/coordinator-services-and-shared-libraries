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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_library(
    name = "test_private_key_client_provider_lib",
    srcs = [
        "//cc/cpio/client_providers/interface:private_key_client_provider_interface.h",
        "//cc/cpio/client_providers/private_key_client_provider/src:private_key_client_provider_srcs",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/message_router/src:message_router_lib",
        "//cc/cpio/client_providers/global_cpio/src:global_cpio_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/cpio/client_providers/kms_client_provider/src:kms_client_provider_lib",
        "//cc/cpio/client_providers/private_key_fetching_client_provider/src:private_key_fetching_client_provider_lib",
        "//cc/public/cpio/interface/private_key_client:type_def",
        "//cc/public/cpio/proto/private_key_service/v1:private_key_service_cc_proto",
        "//cc/public/cpio/proto/public_key_service/v1:public_key_service_cc_proto",
    ],
)

cc_test(
    name = "private_key_client_provider_test",
    size = "small",
    srcs = ["private_key_client_provider_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/message_router/src:message_router_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/kms_client_provider/mock:kms_client_provider_mock",
        "//cc/cpio/client_providers/private_key_client_provider/mock:private_key_client_provider_mock",
        "//cc/cpio/client_providers/private_key_client_provider/src:private_key_client_provider_lib",
        "//cc/cpio/client_providers/private_key_fetching_client_provider/mock:private_key_fetching_client_provider_mock",
        "//cc/public/core/test/interface:execution_result_test_lib",
        "//cc/public/cpio/core/mock:lib_cpio_mock",
        "//cc/public/cpio/proto/private_key_service/v1:private_key_service_cc_proto",
        "//cc/public/cpio/proto/public_key_service/v1:public_key_service_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "private_key_client_utils_test",
    size = "small",
    srcs = ["private_key_client_utils_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/message_router/src:message_router_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/private_key_client_provider/mock:private_key_client_provider_mock",
        "//cc/cpio/client_providers/private_key_client_provider/src:private_key_client_provider_lib",
        "//cc/cpio/proto:cc_proto",
        "//cc/public/cpio/proto/public_key_service/v1:public_key_service_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
