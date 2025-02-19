# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Commonly used copt for benchmarking.

This macro provides a convenient way to enable benchmarking settings in C++ code.

Note: C++ style guides generally discourage the use of preprocessor macros.
This macro should be used only when necessary, such as for guarding performance sensitive code.
"""
BENCHMARK_COPT = select(
    {
        "//cc:enable_benchmarking_setting": [
            "-DPBS_ENABLE_BENCHMARKING=1",
        ],
        "//cc:disable_benchmarking_setting": [],
        "//conditions:default": [],
    },
)
