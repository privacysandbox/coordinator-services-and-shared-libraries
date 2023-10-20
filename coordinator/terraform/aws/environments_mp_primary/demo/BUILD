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
    name = "mp_primary_coordinator_demo",
    srcs = [
        "//coordinator/terraform/aws/environments_mp_primary/demo/adtechmanagement_application:adtechmanagement_demo",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_application:distributedpbs_application_demo",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_application:distributedpbs_application_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_application:distributedpbs_application_outputs_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_application:distributedpbs_application_variables_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_base:distributedpbs_base_demo",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_base:distributedpbs_base_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_base:distributedpbs_base_outputs_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_base:distributedpbs_base_variables_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_prober:distributedpbs_prober_demo",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_prober:distributedpbs_prober_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_prober:distributedpbs_prober_outputs_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/distributedpbs_prober:distributedpbs_prober_variables_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/domainrecords:domain_records_demo_files",
        "//coordinator/terraform/aws/environments_mp_primary/demo/domainrecords:domain_records_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/domainrecords:domain_records_outputs_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/domainrecords:domain_records_vars_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/mpkhs_primary:mpkhs_primary_demo_files",
        "//coordinator/terraform/aws/environments_mp_primary/demo/mpkhs_primary:mpkhs_primary_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/mpkhs_primary:mpkhs_primary_outputs_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/mpkhs_primary:mpkhs_primary_vars_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/roleprovider:role_provider_demo_files",
        "//coordinator/terraform/aws/environments_mp_primary/demo/roleprovider:role_provider_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/roleprovider:role_provider_outputs_link",
        "//coordinator/terraform/aws/environments_mp_primary/demo/roleprovider:role_provider_vars_link",
    ],
    prefix = "environments_mp_primary/demo",
)
