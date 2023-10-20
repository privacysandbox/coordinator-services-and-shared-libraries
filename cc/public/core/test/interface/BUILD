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
    name = "execution_result_matchers",
    srcs = ["execution_result_matchers.cc"],
    hdrs = [
        "execution_result_matchers.h",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/common/proto:core_common_proto_lib",
        "//cc/public/core/interface:errors",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "struct_matchers",
    testonly = True,
    hdrs = [
        "struct_matchers.h",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "@com_google_googletest//:gtest",
    ],
)
