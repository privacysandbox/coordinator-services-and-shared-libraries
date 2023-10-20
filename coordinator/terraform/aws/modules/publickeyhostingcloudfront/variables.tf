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


################################################################################
# Alarm Variables
################################################################################

variable "public_key_service_alarms_enabled" {
  description = "Enable alarms for public key service"
  type        = bool
}

variable "get_public_key_sns_topic_arn" {
  description = "ARN for SNS topic in us-east-1 to send alarm notifications"
  type        = string
}

variable "get_public_key_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "get_public_key_cloudfront_5xx_threshold" {
  description = "5xx error rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

variable "get_public_key_cloudfront_cache_hit_threshold" {
  description = "Cache hits ratio below which to send alarm. Measured in percentage. Example: '99'."
  type        = string
}

variable "get_public_key_cloudfront_origin_latency_threshold" {
  description = "Max origin latency to send alarm. Measured in milliseconds. Example: '5000'."
  type        = string
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}
