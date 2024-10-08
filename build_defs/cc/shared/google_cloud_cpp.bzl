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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def import_google_cloud_cpp():
    # Load the googleapis dependency for gcloud.
    maybe(
        http_archive,
        name = "com_google_googleapis",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googleapis/4d5f3a731f6e4c9dc925830f405cf8869eacc916.tar.gz",
            "https://github.com/googleapis/googleapis/archive/4d5f3a731f6e4c9dc925830f405cf8869eacc916.tar.gz",
        ],
        sha256 = "09d7d33219b630dd7f51bb010c7d32bb6c832fe0c3a7d46ea83a56a34434bd79",
        strip_prefix = "googleapis-4d5f3a731f6e4c9dc925830f405cf8869eacc916",
        build_file = Label("//build_defs/cc/shared/build_targets:googleapis.BUILD"),
    )
    maybe(
        http_archive,
        name = "com_github_googleapis_google_cloud_cpp",
        sha256 = "758e1eca8186b962516c0659b34ce1768ba1c9769cfd998c5bbffb084ad901ff",
        strip_prefix = "google-cloud-cpp-2.29.0",
        url = "https://github.com/googleapis/google-cloud-cpp/archive/v2.29.0.tar.gz",
    )
