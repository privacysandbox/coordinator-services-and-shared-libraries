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

variable "environment" {
  description = "The name of the environment e.g. staging."
  type        = string
  nullable    = false
}

variable "region" {
  description = "The GCP region for this deployment."
  type        = string
  nullable    = false
}

variable "project_id" {
  description = "The GCP project ID to use for this deployment."
  type        = string
  nullable    = false
}

variable "parent_domain_name" {
  description = "The parent domain name used for this PBS environment. NOTE: The hosted zone must exist."
  type        = string
  nullable    = false
}

variable "enable_domain_management" {
  description = "Whether to enable domain management."
  type        = bool
  nullable    = false
}

variable "pbs_domain" {
  description = "The domain used for PBS."
  type        = string
  nullable    = false
}

variable "parent_domain_project" {
  description = "The project owning the parent domain name used for this PBS environment. NOTE: The hosted zone must exist."
  type        = string
  nullable    = false
}

variable "pbs_ip_address" {
  description = "The PBS IP address."
  type        = string
  nullable    = false
}

variable "pbs_auth_cloudfunction_name" {
  description = "The PBS cloud function name."
  type        = string
  nullable    = false
}

variable "pbs_managed_instance_group_url" {
  description = "The URL of the PBS managed instance group."
  type        = string
  nullable    = false
}

variable "pbs_named_port" {
  description = "The name of the port that PBS uses. This is a string value which represents the managed instance group named port. Should map to pbs_main_port."
  type        = string
  nullable    = false
}

variable "pbs_main_port" {
  description = "The port to access the main PBS. Should map to pbs_named_port."
  type        = number
  nullable    = false
}

variable "pbs_health_check_port" {
  description = "The health check port for PBS."
  type        = number
  nullable    = false
}

variable "pbs_vpc_network_id" {
  description = "This is the VPC (google compute network) ID to use for the load balancer."
  type        = string
  nullable    = false
}

variable "pbs_instance_target_tag" {
  description = "The tag added to instances that the load balancer traffic should reach."
  type        = string
  nullable    = false
}

variable "pbs_instance_allow_ssh" {
  description = "Whether to allow ssh traffic through firewall."
  type        = bool
  nullable    = false
}

variable "pbs_tls_alternate_names" {
  description = "PBS Subject Alternative Names for the TLS cert"
  type        = list(string)
  nullable    = true
}