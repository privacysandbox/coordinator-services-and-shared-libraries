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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Load specific version of differential privacy from github.

DIFFERENTIAL_PRIVACY_COMMIT = "68bdbb24fe493638d937120c08927398604c55af"

# value recommended by the differential privacy repo.
# date, not after the specified commit to allow for more shallow clone of repo
# for faster build times.
DIFFERENTIAL_PRIVACY_SHALLOW_SINCE = "1618997113 +0200"

def differential_privacy():
    maybe(
        git_repository,
        name = "com_google_differential_privacy",
        commit = DIFFERENTIAL_PRIVACY_COMMIT,
        remote = "https://github.com/google/differential-privacy.git",
        shallow_since = DIFFERENTIAL_PRIVACY_SHALLOW_SINCE,
    )
