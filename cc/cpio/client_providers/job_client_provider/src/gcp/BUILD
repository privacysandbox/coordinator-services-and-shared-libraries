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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_library(
    name = "gcp_job_client_provider_lib",
    srcs = [
        ":gcp_job_client_provider_srcs",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/client_providers/job_client_provider/src:job_client_provider_lib",
        "//cc/cpio/client_providers/nosql_database_client_provider/src/gcp:gcp_nosql_database_provider_lib",
        "//cc/public/core/interface:execution_result",
    ],
)

exports_files([
    "gcp_job_client_provider.cc",
    "gcp_job_client_provider.h",
])

filegroup(
    name = "gcp_job_client_provider_srcs",
    srcs = [
        ":gcp_job_client_provider.cc",
        ":gcp_job_client_provider.h",
    ],
)
