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
# General Variables
################################################################################

variable "environment" {
  description = "environment where this snstopic is used. Eg: dev/prod, etc"
  type        = string
}

variable "min_cloudfront_ttl_seconds" {
  description = "The minimum amount of time in seconds for CloudFront to cache before origin query, regardless of cache headers."
  type        = number
}

variable "max_cloudfront_ttl_seconds" {
  description = "The maximum amount of time in seconds for CloudFront to cache before origin query, regardless of cache headers."
  type        = number
}

variable "default_cloudfront_ttl_seconds" {
  description = "The amount of time content will be cached if no cache headers are present."
  type        = number
}

################################################################################
# Routing Variables
################################################################################

variable "primary_api_gateway_url" {
  description = "URL for primary origin"
  type        = string
}

variable "secondary_api_gateway_url" {
  description = "URL for failover region"
  type        = string
}

variable "api_gateway_stage_name" {
  description = "Stage name for API gateway instances"
  type        = string
}

variable "enable_domain_management" {
  description = "Manage domain SSL cert creation and routing for public and private key services"
  type        = bool
}

variable "parent_domain_name" {
  description = "custom domain name that redirects to this cloudfront distribution"
  type        = string
}

variable "service_subdomain" {
  description = "Subdomain to use with the domain_name to redirect to this cloudfront distribution."
  type        = string
}

variable "service_alternate_domain_names" {
  description = "An list of alternate domain name for which to add as a subject alternative name in the SSL certificate. Eg: [service-example.com]"
  type        = list(string)
}

variable "domain_hosted_zone_id" {
  description = "Hosted zone for route53 record"
  type        = string
}

variable "domain_name_to_domain_hosted_zone_id" {
  description = "(Optiontal) a Map of domain_names to the hosted zone id it belongs to that should be used to verify the SANs."
  type        = map(string)
}
