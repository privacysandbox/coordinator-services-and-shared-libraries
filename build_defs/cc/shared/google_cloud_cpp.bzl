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
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googleapis/21c839628513a6b8d6ce99ae6911d4d2094c0767.tar.gz",
            "https://github.com/googleapis/googleapis/archive/21c839628513a6b8d6ce99ae6911d4d2094c0767.tar.gz",
        ],
        sha256 = "1e4a8270d73d28d2b276e85eb2af8b956447f1a76823788d625f2c55b06398db",
        strip_prefix = "googleapis-21c839628513a6b8d6ce99ae6911d4d2094c0767",
        build_file = Label("//build_defs/cc/shared/build_targets:googleapis.BUILD"),
    )
    maybe(
        http_archive,
        name = "com_github_googleapis_google_cloud_cpp",
        sha256 = "6f58213e2af16326392da84cd8a52af78cb80bc47338eb87e87d14c14c0e6bad",
        strip_prefix = "google-cloud-cpp-2.25.0",
        url = "https://github.com/googleapis/google-cloud-cpp/archive/v2.25.0.tar.gz",
    )
