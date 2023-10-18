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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "proxy_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
        exclude = [
            "proxy.cc",
            "preload.cc",
            "proxify.cc",
            "socket_vendor.cc",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/aws:include_dir",
        "@boost//:asio",
        "@com_google_googletest//:gtest_main",
    ],
)

# This is used by tests to verify the functionality of this file. Not built to
# binaries.
cc_library(
    name = "preload",
    srcs = [
        ":preload.cc",
        ":protocol.cc",
        ":protocol.h",
        ":socket_vendor_protocol.h",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = ["//cc/aws:include_dir"],
)

cc_binary(
    name = "proxy",
    srcs = [":proxy.cc"],
    copts = [
        "-std=c++17",
    ],
    linkopts = ["-pthread"],
    deps = [
        ":proxy_lib",
    ],
)

cc_binary(
    name = "proxy_preload",
    srcs = [
        ":preload.cc",
        ":protocol.cc",
        ":protocol.h",
        ":socket_vendor_protocol.h",
    ],
    copts = [
        "-fvisibility=hidden",
        "-std=c++17",
    ],
    linkopts = ["-ldl"],
    linkshared = True,
    deps = ["//cc/aws:include_dir"],
)

cc_binary(
    name = "proxify",
    srcs = [":proxify.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = ["//cc/aws:include_dir"],
)

cc_binary(
    name = "socket_vendor",
    srcs = [":socket_vendor.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        ":proxy_lib",
    ],
)
