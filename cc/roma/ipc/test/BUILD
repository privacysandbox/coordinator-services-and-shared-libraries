# Copyright 2023 Google LLC
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

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "ipc_manager_test",
    size = "small",
    srcs = ["ipc_manager_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/ipc/src:roma_ipc_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ipc_message_test",
    size = "small",
    srcs = ["ipc_message_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/ipc/src:roma_ipc_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ipc_channel_test",
    size = "small",
    srcs = ["ipc_channel_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/ipc/src:roma_ipc_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "work_container_test",
    size = "small",
    srcs = ["work_container_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/ipc/src:roma_ipc_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
