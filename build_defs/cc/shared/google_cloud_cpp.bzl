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
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googleapis/b6b4ed5d7ebdfc0ff8855311bd63ba258b94ae2b.tar.gz",
            "https://github.com/googleapis/googleapis/archive/b6b4ed5d7ebdfc0ff8855311bd63ba258b94ae2b.tar.gz",
        ],
        sha256 = "faefdf45528de8c5c9a8a0454134c2f9a0916fbd55ccae1d2b15af9cbee30ee6",
        strip_prefix = "googleapis-b6b4ed5d7ebdfc0ff8855311bd63ba258b94ae2b",
        build_file = Label("//build_defs/cc/shared/build_targets:googleapis.BUILD"),
    )
    maybe(
        http_archive,
        name = "com_github_googleapis_google_cloud_cpp",
        sha256 = "21fb441b5a670a18bb16b6826be8e0530888d0b94320847c538d46f5a54dddbc",
        strip_prefix = "google-cloud-cpp-2.8.0",
        url = "https://github.com/googleapis/google-cloud-cpp/archive/v2.8.0.tar.gz",
    )
