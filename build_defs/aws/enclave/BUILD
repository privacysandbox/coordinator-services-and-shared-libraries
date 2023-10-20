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

load(":allocator.bzl", "allocator_yaml")

package(default_visibility = ["//visibility:public"])

exports_files([
    "allocator.template.yaml",
    "generic_enclave_ami.pkr.hcl",
])

# Small allocator.yaml file suitable for running on a c5.xlarge (4 host cpus,
# 8GB host RAM).
allocator_yaml(
    name = "small.allocator.yaml",
    enclave_cpus = 2,
    enclave_memory_mib = 7168,
)
