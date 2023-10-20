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

# google cloud sdk for releasing artifacts to gcs

def google_cloud_sdk():
    maybe(
        http_archive,
        name = "google-cloud-sdk",
        build_file_content = """
package(default_visibility = ["//visibility:public"])
exports_files(["google-cloud-sdk"])
""",
        # latest from https://cloud.google.com/storage/docs/gsutil_install#linux as of 2021-12-16
        sha256 = "94328b9c6559a1b7ec2eeaab9ef0e4702215e16e8327c5b99718750526ae1efe",
        url = "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-sdk-367.0.0-linux-x86_64.tar.gz",
    )
