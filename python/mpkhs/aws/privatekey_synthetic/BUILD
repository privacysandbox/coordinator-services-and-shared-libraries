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

load("@py3_mpkhs_aws_privatekey_synthetic_deps//:requirements.bzl", "requirement")
load("@rules_pkg//:mappings.bzl", "pkg_files")
load("@rules_pkg//:pkg.bzl", "pkg_zip")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "privatekey_heartbeat",
    srcs = ["privatekey_heartbeat.py"],
)

pkg_files(
    name = "aws_requests_auth_srcs",
    srcs = [
        requirement("aws_requests_auth"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "certifi_srcs",
    srcs = [
        requirement("certifi"),
    ],
    strip_prefix = "site-packages",
)

genrule(
    name = "certifi_cert_http",
    srcs = ["@py3_certifi_cert//file"],
    outs = ["cacert.pem"],
    cmd = "cp $< $@",
)

pkg_files(
    name = "certifi_cert_srcs",
    srcs = [
        "certifi_cert_http",
    ],
    prefix = "certifi",
)

genrule(
    name = "hyper_cert_http",
    srcs = ["@py3_hyper_cert//file"],
    outs = ["certs.pem"],
    cmd = "cp $< $@",
)

pkg_files(
    name = "hyper_cert_srcs",
    srcs = [
        "hyper_cert_http",
    ],
    prefix = "hyper",
)

pkg_files(
    name = "requests_srcs",
    srcs = [
        requirement("requests"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "urllib3_srcs",
    srcs = [
        requirement("urllib3"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "awscrt_srcs",
    srcs = [
        requirement("awscrt"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "hyper_srcs",
    srcs = [
        requirement("hyper"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "hyperframe_srcs",
    srcs = [
        requirement("hyperframe"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "hpack_srcs",
    srcs = [
        requirement("hpack"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "h2_srcs",
    srcs = [
        requirement("h2"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "idna_srcs",
    srcs = [
        requirement("idna"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "charset-normalizer_srcs",
    srcs = [
        requirement("charset-normalizer"),
    ],
    strip_prefix = "site-packages",
)

pkg_files(
    name = "privatekey_heartbeat_srcs",
    srcs = [
        "privatekey_heartbeat.py",
    ],
    prefix = "heartbeat",
)

pkg_zip(
    name = "privatekey_heartbeat_pkg",
    srcs = [
        ":aws_requests_auth_srcs",
        ":awscrt_srcs",
        ":certifi_cert_srcs",
        ":certifi_srcs",
        ":charset-normalizer_srcs",
        ":h2_srcs",
        ":hpack_srcs",
        ":hyper_cert_srcs",
        ":hyper_srcs",
        ":hyperframe_srcs",
        ":idna_srcs",
        ":privatekey_heartbeat_srcs",
        ":requests_srcs",
        ":urllib3_srcs",
    ],
    package_dir = "python",
)
