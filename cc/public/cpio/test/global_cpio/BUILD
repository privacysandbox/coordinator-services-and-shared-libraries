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
    name = "test_cpio_options",
    hdrs = [
        "test_cpio_options.h",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/public/cpio/interface:type_def",
    ],
)

cc_library(
    name = "test_lib_cpio",
    srcs = [
        "test_lib_cpio.cc",
        "//cc/public/cpio/core/src:cpio.cc",
    ],
    hdrs = [
        "test_lib_cpio.h",
        "//cc/public/cpio/interface:cpio.h",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/logger/src:logger_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/logger/src/log_providers/syslog:syslog_lib",
        "//cc/cpio/client_providers/cloud_initializer/src:cloud_initializer_select_lib",
        "//cc/cpio/client_providers/global_cpio/test:test_global_cpio_lib",
        "//cc/cpio/client_providers/global_cpio/test:test_lib_cpio_provider",
        "//cc/public/cpio/core/src:cpio_utils_lib",
    ],
)
