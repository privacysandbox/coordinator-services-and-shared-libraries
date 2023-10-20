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

variable "service_domain_name" {
  description = "Specific domain name for which to create an SSL certificate. Eg: service-example.com"
  type        = string
}

variable "subject_alternative_names" {
  description = "(Optiontal) Set of domains that should be SANs in the issued certificate."
  type        = list(string)
  default     = []
}

variable "domain_hosted_zone_id" {
  description = "Hosted zone for route53 record"
  type        = string
}

variable "domain_name_to_domain_hosted_zone_id" {
  description = "(Optiontal) a Map of domain_names to the hosted zone id it belongs to that should be used to verify the SANs."
  type        = map(string)
  default     = {}
}

variable "environment" {
  description = "environment where this service is deployed. Eg: dev/prod, etc"
  type        = string
}
