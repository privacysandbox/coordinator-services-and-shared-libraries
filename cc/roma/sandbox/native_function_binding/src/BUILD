# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "roma_native_function_binding_lib",
    srcs = [
        "native_function_invoker.h",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/roma/interface:roma_function_binding_io_cc_proto",
    ],
)

cc_library(
    name = "roma_native_function_invoker_sapi_ipc_lib",
    srcs = [
        "error_codes.h",
        "native_function_invoker_sapi_ipc.cc",
        "native_function_invoker_sapi_ipc.h",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        ":roma_native_function_binding_lib",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/public/core/interface:execution_result",
        "//cc/roma/interface:roma_function_binding_io_cc_proto",
        "//cc/roma/sandbox/constants:roma_constants_lib",
        "@com_google_sandboxed_api//sandboxed_api:sapi",
        "@com_google_sandboxed_api//sandboxed_api:vars",
    ],
)

cc_library(
    name = "roma_native_function_handler_sapi_ipc_lib",
    srcs = [
        "error_codes.h",
        "native_function_handler_sapi_ipc.cc",
        "native_function_handler_sapi_ipc.h",
        "native_function_table.cc",
        "native_function_table.h",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/public/core/interface:execution_result",
        "//cc/roma/interface:roma_function_binding_io_cc_proto",
        "//cc/roma/sandbox/constants:roma_constants_lib",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_sandboxed_api//sandboxed_api:sapi",
        "@com_google_sandboxed_api//sandboxed_api:vars",
    ],
)
