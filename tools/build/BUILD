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

load("@rules_python//python:defs.bzl", "py_runtime", "py_runtime_pair")

py_runtime(
    name = "py_wrapper_runtime",
    interpreter = "py_wrapper.sh",
    python_version = "PY3",
)

py_runtime_pair(
    name = "py_wrapper_runtime_pair",
    py2_runtime = None,
    py3_runtime = ":py_wrapper_runtime",
)

toolchain(
    name = "py_wrapper_toolchain",
    toolchain = ":py_wrapper_runtime_pair",
    toolchain_type = "@rules_python//python:toolchain_type",
)
