# Copyright 2023 Google LLC
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

load("//build_defs/cc:google_benchmark.bzl", "google_benchmark")
load("//build_defs/cc:v8.bzl", "import_v8")
load("//build_defs/cc/shared:sandboxed_api.bzl", "sandboxed_api")

def roma_dependencies(scp_repo_name = ""):
    import_v8(scp_repo_name)
    sandboxed_api(scp_repo_name)
    google_benchmark()
