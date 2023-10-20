/**
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

module "operator_service" {
  source      = "../../applications/cc_operator_service"
  environment = var.environment
  project_id  = var.project_id
  region      = var.region
  region_zone = var.region_zone

  # Global Alarms
  alarms_enabled            = var.alarms_enabled
  alarms_notification_email = var.alarms_notification_email

  # Spanner DB
  spanner_instance_config              = var.spanner_instance_config
  spanner_processing_units             = var.spanner_processing_units
  spanner_database_deletion_protection = var.spanner_database_deletion_protection

  # Worker
  instance_type                    = var.instance_type
  instance_disk_image              = var.instance_disk_image
  user_provided_worker_sa_email    = var.user_provided_worker_sa_email
  worker_instance_force_replace    = var.worker_instance_force_replace
  worker_logging_enabled           = var.worker_logging_enabled
  worker_monitoring_enabled        = var.worker_monitoring_enabled
  worker_image                     = var.worker_image
  worker_restart_policy            = var.worker_restart_policy
  allowed_operator_service_account = var.allowed_operator_service_account

  # Worker Alarms
  worker_alarm_duration_sec    = var.worker_alarm_duration_sec
  worker_alarm_eval_period_sec = var.worker_alarm_eval_period_sec

  # Autoscaling
  min_worker_instances          = var.min_worker_instances
  max_worker_instances          = var.max_worker_instances
  autoscaling_jobs_per_instance = var.autoscaling_jobs_per_instance

  # Autoscaling Alarms
  autoscaling_alarm_duration_sec               = var.autoscaling_alarm_duration_sec
  autoscaling_alarm_eval_period_sec            = var.autoscaling_alarm_eval_period_sec
  autoscaling_max_vm_instances_ratio_threshold = var.autoscaling_max_vm_instances_ratio_threshold

  # Job Queue Alarms
  jobqueue_alarm_eval_period_sec           = var.jobqueue_alarm_eval_period_sec
  jobqueue_max_undelivered_message_age_sec = var.jobqueue_max_undelivered_message_age_sec

  # VPC Service Control
  vpcsc_compatible           = var.vpcsc_compatible
  vpc_connector_machine_type = var.vpc_connector_machine_type

  # Frontend Service
  operator_package_bucket_location             = var.operator_package_bucket_location
  frontend_service_jar                         = var.frontend_service_jar
  frontend_service_cloudfunction_memory_mb     = var.frontend_service_cloudfunction_memory_mb
  frontend_service_cloudfunction_min_instances = var.frontend_service_cloudfunction_min_instances
  frontend_service_cloudfunction_max_instances = var.frontend_service_cloudfunction_max_instances
  frontend_service_cloudfunction_timeout_sec   = var.frontend_service_cloudfunction_timeout_sec
  job_table_ttl_days                           = var.job_table_ttl_days
  job_version                                  = var.job_version

  # Frontend Service Alarms
  frontend_alarm_duration_sec                   = var.frontend_alarm_duration_sec
  frontend_alarm_eval_period_sec                = var.frontend_alarm_eval_period_sec
  frontend_cloudfunction_5xx_threshold          = var.frontend_cloudfunction_5xx_threshold
  frontend_cloudfunction_error_threshold        = var.frontend_cloudfunction_error_threshold
  frontend_cloudfunction_max_execution_time_max = var.frontend_cloudfunction_max_execution_time_max
  frontend_lb_5xx_threshold                     = var.frontend_lb_5xx_threshold
  frontend_lb_max_latency_ms                    = var.frontend_lb_max_latency_ms
}
