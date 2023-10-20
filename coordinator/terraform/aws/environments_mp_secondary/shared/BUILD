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

load("@rules_pkg//:mappings.bzl", "pkg_filegroup")

package(default_visibility = ["//visibility:public"])

pkg_filegroup(
    name = "mp_secondary_coordinator_shared",
    srcs = [
        "//coordinator/terraform/aws/environments_mp_secondary/shared/adtechmanagement_application:adtechmanagement_shared",
        "//coordinator/terraform/aws/environments_mp_secondary/shared/distributedpbs_application:distributedpbs_application_shared",
        "//coordinator/terraform/aws/environments_mp_secondary/shared/distributedpbs_base:distributedpbs_base_shared",
        "//coordinator/terraform/aws/environments_mp_secondary/shared/distributedpbs_prober:distributedpbs_prober_shared",
        "//coordinator/terraform/aws/environments_mp_secondary/shared/domainrecords:domain_records_shared",
        "//coordinator/terraform/aws/environments_mp_secondary/shared/mpkhs_secondary:mpkhs_secondary_shared",
        "//coordinator/terraform/aws/environments_mp_secondary/shared/roleprovider:role_provider_shared",
    ],
    prefix = "environments_mp_secondary/shared",
)
