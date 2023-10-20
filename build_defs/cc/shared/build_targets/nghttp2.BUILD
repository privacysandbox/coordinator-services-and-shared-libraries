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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "nghttp2",
    srcs = glob([
        "lib/*.c",
        "lib/*.h",
    ]),
    hdrs = glob([
        "lib/includes/nghttp2/*.h",
    ]),
    copts = [
        "-DHAVE_CONFIG_H",
        "-DBUILDING_NGHTTP2",
        "-Wno-string-plus-int",
    ],
    includes = [
        "lib",
        "lib/includes",
    ],
)

cc_library(
    name = "llhttp",
    srcs = [
        "third-party/llhttp/src/api.c",
        "third-party/llhttp/src/http.c",
        "third-party/llhttp/src/llhttp.c",
    ],
    hdrs = [
        "third-party/llhttp/include/llhttp.h",
    ],
    includes = [
        "third-party/llhttp/include",
    ],
)

cc_library(
    name = "url_parser",
    srcs = [
        "third-party/url-parser/url_parser.c",
    ],
    hdrs = [
        "third-party/url-parser/url_parser.h",
    ],
    includes = [
        "third-party",
    ],
)

NGHTTP2_ASIO_SOURCES_NO_HEADER = [
    "src/asio_client_request.cc",
    "src/asio_client_response.cc",
    "src/asio_client_session.cc",
    "src/asio_server_http2.cc",
    "src/asio_server_request.cc",
    "src/asio_server_response.cc",
]

NGHTTP2_ASIO_SOURCES_HEADER = [
    "src/allocator.h",
    "src/asio_server_connection.h",
    "src/base64.h",
    "src/buffer.h",
    "src/memchunk.h",
    "src/nghttp2_config.h",
    "src/network.h",
    "src/ssl_compat.h",
    "src/template.h",
]

NGHTTP2_ASIO_C_SOURCES_HEADERS = [
    "src/timegm",
]

NGHTTP2_ASIO_CC_SOURCES_HEADERS = [
    "src/http2",
    "src/tls",
    "src/util",
    "src/asio_common",
    "src/asio_io_service_pool",
    "src/asio_server_http2_impl",
    "src/asio_server",
    "src/asio_server_http2_handler",
    "src/asio_server_request_impl",
    "src/asio_server_response_impl",
    "src/asio_server_stream",
    "src/asio_server_serve_mux",
    "src/asio_server_request_handler",
    "src/asio_server_tls_context",
    "src/asio_client_session_impl",
    "src/asio_client_session_tcp_impl",
    "src/asio_client_session_tls_impl",
    "src/asio_client_response_impl",
    "src/asio_client_request_impl",
    "src/asio_client_stream",
    "src/asio_client_tls_context",
]

cc_library(
    name = "nghttp2_asio",
    srcs = ["{}.c".format(s) for s in NGHTTP2_ASIO_C_SOURCES_HEADERS] +
           ["{}.cc".format(s) for s in NGHTTP2_ASIO_CC_SOURCES_HEADERS] +
           NGHTTP2_ASIO_SOURCES_NO_HEADER,
    hdrs = glob(["src/includes/nghttp2/*.h"]) + [
               "lib/config.h",
           ] + NGHTTP2_ASIO_SOURCES_HEADER +
           ["{}.h".format(s) for s in NGHTTP2_ASIO_C_SOURCES_HEADERS + NGHTTP2_ASIO_CC_SOURCES_HEADERS],
    copts = [
        "-DHAVE_CONFIG_H",
        "-DBUILDING_NGHTTP2",
        "-Wno-string-plus-int",
        "-I.",
        "-Isrc",
    ],
    includes = [
        "src/includes",
    ],
    deps = [
        ":llhttp",
        ":nghttp2",
        ":url_parser",
        "@boost//:asio_ssl",
        "@boost//:thread",
    ],
)
