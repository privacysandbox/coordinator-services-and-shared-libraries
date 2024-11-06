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
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googleapis/596e2a047b7d604f922f740612554f84f9a8fa8d.tar.gz",
            "https://github.com/googleapis/googleapis/archive/596e2a047b7d604f922f740612554f84f9a8fa8d.tar.gz",
        ],
        sha256 = "7f450e5c7b49af69fc265abfcea15e8e0a983a777d84bf52133e320015a6157c",
        strip_prefix = "googleapis-596e2a047b7d604f922f740612554f84f9a8fa8d",
        build_file = Label("//build_defs/cc/shared/build_targets:googleapis.BUILD"),
    )
    maybe(
        http_archive,
        name = "com_github_googleapis_google_cloud_cpp",
        sha256 = "170650b11ece54977b42dd85be648b6bd2d614ff68ea6863a0013865e576b49c",
        strip_prefix = "google-cloud-cpp-2.30.0",
        url = "https://github.com/googleapis/google-cloud-cpp/archive/v2.30.0.tar.gz",
    )
