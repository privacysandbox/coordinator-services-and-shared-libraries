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

#TODO(b/240608110) Switch to environment
variable "environment_prefix" {
  description = "Value used to prefix the name of the created resources."
  type        = string
  nullable    = false
}

variable "auth_lambda_handler_path" {
  description = <<-EOT
        Path to the python auth lambda handler zip.

        When deployed with multiparty_coordinator_tar.tgz, this is extracted
        at "dist/pbs_auth_handler_lambda.zip".

        When developing within the scp project itself, it is found in
        "//bazel-bin/python/privacybudget/aws/pbs_auth_handler/pbs_auth_handler_lambda.zip"
      EOT
  type        = string
  nullable    = false
}

variable "auth_dynamo_db_table_name" {
  type     = string
  nullable = false
}

variable "pbs_authorization_v2_table_name" {
  type     = string
  nullable = false
}

variable "auth_dynamo_db_table_arn" {
  type     = string
  nullable = false
}

variable "pbs_authorization_v2_table_arn" {
  type     = string
  nullable = false
}

variable "enable_domain_management" {
  type     = bool
  default  = false
  nullable = false
}

variable "parent_domain_name" {
  description = "Custom domain name that redirects to the PBS environment"
  type        = string
  default     = ""
}

variable "service_subdomain" {
  description = "Subdomain to use with the parent_domain_name to designate the PBS endpoint. This will be prepended with auth."
  type        = string
  default     = "mp-pbs"
}

variable "domain_hosted_zone_id" {
  description = "Hosted zone for route53 record"
  type        = string
}
