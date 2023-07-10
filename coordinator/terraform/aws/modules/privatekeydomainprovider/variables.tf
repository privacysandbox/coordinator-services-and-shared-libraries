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

################################################################################
# Global Variables
################################################################################

variable "environment" {
  description = "environment where this service is deployed. Eg: dev/prod, etc"
  type        = string
}

variable "parent_domain_name" {
  description = "custom domain name that redirects to this API"
  type        = string
}

variable "service_subdomain" {
  description = "subdomain to use with the domain_name to designate this API gateway instance."
  type        = string
}

variable "domain_hosted_zone_id" {
  description = "Hosted zone for route53 record"
  type        = string
}

variable "get_private_key_api_gateway_id" {
  description = "Private key API Gateway instance"
  type        = string
}

variable "get_private_key_api_gateway_stage_id" {
  description = "Private key API Gateway stage"
  type        = string
}
