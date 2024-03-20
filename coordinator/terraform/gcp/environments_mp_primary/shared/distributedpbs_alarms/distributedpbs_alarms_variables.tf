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

########################
# DO NOT EDIT MANUALLY #
########################

# This file is meant to be shared across all environments (either copied or
# symlinked). In order to make the upgrade process easier, this file should not
# be modified for environment-specific customization.

################################################################################
# Global Variables.
################################################################################

variable "project" {
  description = "The GCP project name to use."
  type        = string
}

variable "region" {
  description = "Region where all resources will be created."
  type        = string
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

################################################################################
# Alarms Variables.
################################################################################

variable "alarms_notification_email" {
  description = "Email to receive alarms for pbs services."
  type        = string
}

variable "eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
  default     = "60"
}

variable "duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = string
  default     = "60"
}

variable "error_log_threshold" {
  description = "Error log count greater than this to send alarm. Example: 0."
  type        = number
  default     = 0
}

variable "critical_log_threshold" {
  description = "Critical log count greater than this to send alarm. Example: 0."
  type        = number
  default     = 0
}

variable "alert_log_threshold" {
  description = "Log error count greater than this to send alarm. Example: 0."
  type        = number
  default     = 0
}

variable "emergency_log_threshold" {
  description = " Emergency log count greater than this to send alarm. Example: 0."
  type        = number
  default     = 0
}
