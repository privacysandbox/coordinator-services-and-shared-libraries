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

exports_files([
    "lib_cpio_provider.h",
    "lib_cpio_provider.cc",
])

filegroup(
    name = "lib_cpio_provider_srcs",
    srcs = [
        ":lib_cpio_provider.cc",
        ":lib_cpio_provider.h",
    ],
)

cc_library(
    name = "cpio_provider_lib",
    srcs = select({
        "//cc/public/cpio/interface:aws_cpio_lib": [":lib_cpio_provider_srcs"],
        "//conditions:default": [":lib_cpio_provider_srcs"],
    }) + ["//cc/cpio/client_providers/interface:cpio_provider_interface.h"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/curl_client/src:http1_curl_client_lib",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/logger/src:logger_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/logger/src/log_providers/syslog:syslog_lib",
        "//cc/core/message_router/src:message_router_lib",
        "//cc/cpio/client_providers/auth_token_provider/src:auth_token_provider_lib",
        "//cc/cpio/client_providers/instance_client_provider_new/src:instance_client_provider_lib",
        "//cc/cpio/client_providers/role_credentials_provider/src:role_credentials_provider_lib",
        "//cc/public/cpio/interface:type_def",
    ],
)
