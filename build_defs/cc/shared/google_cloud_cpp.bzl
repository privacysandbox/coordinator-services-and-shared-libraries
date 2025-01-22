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
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googleapis/07737e56be021ca2d11a24fb759ff3de79d83fa0.tar.gz",
            "https://github.com/googleapis/googleapis/archive/07737e56be021ca2d11a24fb759ff3de79d83fa0.tar.gz",
        ],
        sha256 = "2778d41534a730ef33c56aad30790593c59b025a58ac39cba322e55f9148dad6",
        strip_prefix = "googleapis-07737e56be021ca2d11a24fb759ff3de79d83fa0",
        build_file = Label("//build_defs/cc/shared/build_targets:googleapis.BUILD"),
    )
    maybe(
        http_archive,
        name = "com_github_googleapis_google_cloud_cpp",
        sha256 = "22deeb6c2abf0838f4d4c6100e83489bb581fa8015180370500ad31712f601ac",
        strip_prefix = "google-cloud-cpp-2.34.0",
        url = "https://github.com/googleapis/google-cloud-cpp/archive/v2.34.0.tar.gz",
    )
