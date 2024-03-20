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

load("@py3_privacybudget_gcp_operator_onboarding_deps//:requirements.bzl", "requirement")
load("@rules_pkg//:mappings.bzl", "pkg_files")
load("@rules_python//python:defs.bzl", "py_test")

package(default_visibility = ["//visibility:public"])

py_test(
    name = "add_allowed_identities_test",
    size = "small",
    srcs = [
        "add_allowed_identities.py",
        "add_allowed_identities_test.py",
        "google/cloud.py",
    ],
    data = ["data/test_allowed_operators.csv"],
    deps = [requirement("pandas")],
)

pkg_files(
    name = "add_allowed_identities",
    srcs = [
        "README.md",
        "add_allowed_identities.py",
        "requirements.txt",
    ],
)
