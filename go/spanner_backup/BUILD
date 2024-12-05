# Copyright 2024 Google LLC
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

load("@io_bazel_rules_go//go:def.bzl", "go_library", "go_test")

go_library(
    name = "spanner_backup",
    srcs = ["backups.go"],
    importpath = "github.com/privacysandbox/coordinator-services-and-shared-libraries/go/spanner_backup",
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_googleapis_gax_go_v2//:gax-go",
        "@com_google_cloud_go_spanner//admin/database/apiv1",
    ],
)

go_test(
    name = "spanner_backup_test",
    srcs = ["backups_test.go"],
    embed = [":spanner_backup"],
    deps = [
        "@com_github_googleapis_gax_go_v2//:gax-go",
        "@com_github_stretchr_testify//assert",
        "@com_google_cloud_go_spanner//admin/database/apiv1",
    ],
)
