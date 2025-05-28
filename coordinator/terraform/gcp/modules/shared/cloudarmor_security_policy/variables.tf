# Copyright 2025 Google LLC
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

variable "project_id" {
  description = "The GCP project ID."
  type        = string
}

variable "security_policy_name" {
  description = "The name of the security policy."
  type        = string
}

variable "security_policy_description" {
  description = "A description for the security policy."
  type        = string
}

variable "use_adaptive_protection" {
  description = "Whether Cloud Armor Adaptive Protection is being used or not."
  type        = bool
}

variable "ddos_thresholds" {
  description = "An object containing adaptive protection threshold configuration values."
  type = object({
    name                               = string
    detection_load_threshold           = number
    detection_absolute_qps             = number
    detection_relative_to_baseline_qps = number
  })
}

variable "security_policy_rules" {
  description = "Set of objects to define as security policy rules."
  type = set(object({
    description = string
    action      = string
    priority    = number
    preview     = bool
    match = object({
      versioned_expr = string
      config = object({
        src_ip_ranges = list(string)
      })
      expr = object({
        expression = string
      })
    })
  }))
}
