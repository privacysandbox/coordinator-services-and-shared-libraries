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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "roma_interface_lib",
    srcs = glob([
        "*.cc",
        "*.h",
    ]),
    copts = [
        "-std=c++17",
    ],
    includes = ["external/com_google_absl"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/roma/config/src:roma_config_lib",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

proto_library(
    name = "roma_function_binding_io_proto",
    srcs = ["function_binding_io.proto"],
)

cc_proto_library(
    name = "roma_function_binding_io_cc_proto",
    deps = [":roma_function_binding_io_proto"],
)
