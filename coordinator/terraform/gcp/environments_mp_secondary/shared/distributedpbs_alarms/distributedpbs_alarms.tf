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

module "distributedpbs_alarms" {
  source                    = "../../../applications/distributedpbs_alarms"
  alarms_notification_email = var.alarms_notification_email
  eval_period_sec           = var.eval_period_sec
  duration_sec              = var.duration_sec
  error_log_threshold       = var.error_log_threshold
  critical_log_threshold    = var.critical_log_threshold
  alert_log_threshold       = var.alert_log_threshold
  emergency_log_threshold   = var.emergency_log_threshold
  environment               = var.environment
  project                   = var.project
  region                    = var.region
}
