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

load("@py3_privacybudget_gcp_pbs_auth_handler_test_deps//:requirements.bzl", "requirement")
load("@rules_pkg//:mappings.bzl", "pkg_files")
load("@rules_python//python:defs.bzl", "py_test")

package(default_visibility = ["//visibility:public"])

py_test(
    name = "auth_cloud_function_handler_test",
    size = "small",
    srcs = [
        "auth_cloud_function_handler.py",
        "auth_cloud_function_handler_test.py",
        "functions_framework.py",
        "google/cloud.py",
    ],
    deps = [requirement("publicsuffixlist")],
)

pkg_files(
    name = "auth_cloud_function_handler",
    srcs = [
        "auth_cloud_function_handler.py",
        "config/requirements.txt",
    ],
    prefix = "dist/pbs_auth_handler",
)
