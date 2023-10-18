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

load("@com_google_sandboxed_api//sandboxed_api/bazel:proto.bzl", "sapi_proto_library")
load("@com_google_sandboxed_api//sandboxed_api/bazel:sapi.bzl", "sapi_library")
load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//cc/roma/sandbox/worker_api:__subpackages__"])

sapi_proto_library(
    name = "worker_params_proto",
    srcs = ["worker_params.proto"],
    alwayslink = 1,
)

sapi_proto_library(
    name = "worker_init_params_proto",
    srcs = ["worker_init_params.proto"],
    alwayslink = 1,
)

cc_library(
    name = "roma_worker_wrapper_lib",
    srcs = [
        "error_codes.h",
        "worker_wrapper.cc",
        "worker_wrapper.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":worker_init_params_cc_proto",
        ":worker_params_cc_proto",
        "//cc:cc_base_include_dir",
        "//cc/roma/config/src:roma_config_lib",
        "//cc/roma/logging/src:roma_logging_lib",
        "//cc/roma/sandbox/worker_factory/src:roma_worker_factory_lib",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

sapi_library(
    name = "roma_worker_wrapper_lib-sapi",
    hdrs = ["worker_wrapper.h"],
    functions = [
        "InitFromSerializedData",
        "Run",
        "RunCodeFromSerializedData",
        "Stop",
    ],
    generator_version = 1,
    input_files = ["worker_wrapper.cc"],
    lib = ":roma_worker_wrapper_lib",
    lib_name = "WorkerWrapper",
    namespace = "google::scp::roma::sandbox::worker_api",
    deps = [
        ":worker_init_params_cc_proto",
        ":worker_params_cc_proto",
        "//cc/core/interface:interface_lib",
    ],
)

cc_library(
    name = "roma_worker_sandbox_api_lib",
    srcs = [
        "error_codes.h",
        "worker_sandbox_api.cc",
        "worker_sandbox_api.h",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        ":roma_worker_wrapper_lib-sapi",
        ":worker_init_params_cc_proto",
        ":worker_params_cc_proto",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/roma/logging/src:roma_logging_lib",
        "//cc/roma/sandbox/worker_factory/src:roma_worker_factory_lib",
    ],
)
