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

locals {
  function_name      = google_cloudfunctions2_function.encryption_key_service_cloudfunction.name
  load_balancer_name = google_compute_url_map.encryption_key_service_loadbalancer.name
}

module "encryptionkeyservice_loadbalancer_alarms" {
  source = "../shared/loadbalancer_alarms"
  count  = var.alarms_enabled ? 1 : 0

  environment             = var.environment
  notification_channel_id = var.notification_channel_id
  load_balancer_name      = local.load_balancer_name
  service_prefix          = "${var.environment} Encryption Key Service"

  eval_period_sec     = var.alarm_eval_period_sec
  error_5xx_threshold = var.lb_5xx_threshold
  max_latency_ms      = var.lb_max_latency_ms
  duration_sec        = var.alarm_duration_sec
}

module "encryptionkeyservice_cloudfunction_alarms" {
  source = "../shared/cloudfunction_alarms"
  count  = var.alarms_enabled ? 1 : 0

  environment             = var.environment
  notification_channel_id = var.notification_channel_id
  function_name           = local.function_name
  service_prefix          = "${var.environment} Encryption Key Service"

  eval_period_sec           = var.alarm_eval_period_sec
  error_5xx_threshold       = var.cloudfunction_5xx_threshold
  execution_time_max        = var.cloudfunction_max_execution_time_max
  execution_error_threshold = var.cloudfunction_error_threshold
  duration_sec              = var.alarm_duration_sec
}

module "encryptionkeyservice_monitoring_dashboard" {
  source        = "../shared/cloudfunction_dashboards"
  environment   = var.environment
  service_name  = "Encryption Key"
  function_name = local.function_name
}
