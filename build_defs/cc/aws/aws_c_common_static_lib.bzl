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

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def aws_c_common():
    # This local repository is pre-built in docker image and only works for builds within the container.
    maybe(
        native.new_local_repository,
        name = "aws_c_common",
        build_file = "//build_defs/cc/aws:aws_c_common_static_lib.BUILD",
        path = "/opt/deps",
    )
