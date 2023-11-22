# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

variable "environment" {
  description = "environment where this service is deployed. Eg: dev/prod, etc"
  type        = string
}

variable "sns_topic_arn" {
  description = "SNS topic ARN to forward alerts to"
  type        = string
}

variable "pbs_cloudwatch_log_group_name" {
  description = "Pbs cloud watch log group name to create alerts for."
  type        = string
}

variable "eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}

################################################################################
# Log based Alarm Variables.
################################################################################

variable "error_log_log_corrupted_threshold" {
  description = "Alarming threshold for the the count of logs containing ERROR - metric_filter_log_corrupted. Example: '0'."
  type        = string
}

variable "error_log_missing_transaction_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_missing_transaction. Example: '0'."
  type        = string
}

variable "error_log_checkpointing_threshold" {
  description = "Alarming threshold for the the count of logs containing ERROR - metric_filter_checkpointing. Example: '0'."
  type        = string
}

variable "error_log_database_read_threshold" {
  description = "Alarming threshold for the the count of logs containing ERROR - metric_filter_database_read. Example: '0'."
  type        = string
}

variable "error_log_database_update_threshold" {
  description = "Alarming threshold for the the count of logs containing ERROR - metric_filter_database_update. Example: '0'."
  type        = string
}

variable "error_log_missing_component_id_threshold" {
  description = "Alarming threshold for the the count of logs containing ERROR - metric_filter_missing_component_id. Example: '0'."
  type        = string
}

variable "error_log_handle_journal_threshold" {
  description = "Alarming threshold for the the count of logs containing ERROR - metric_filter_handle_journal. Example: '0'."
  type        = string
}

################################################################################
# ELB based Alarm Variables.
################################################################################

variable "loadbalancer" {
  description = "Loadbalancer of the ELB intance for PBS endpoint to create alarms for"
  type        = string
}

variable "error_ratio_4xx_threshold" {
  description = "ELB 4xx error ratio rate greater than this to send alarm. Value should be a percentage. Example: 10% is '10.0'."
  type        = string
}

variable "error_ratio_5xx_threshold" {
  description = "ELB 5xx error ratio rate greater than this to send alarm. Value should be a percentage. Example: 10% is '10.0'."
  type        = string
}

################################################################################
# Dynamo Alarm Variables.
################################################################################

variable "reporting_origin_table_name" {
  description = "Name of the reporting origin table in auth db"
  type        = string
}

variable "pbs_authorization_v2_table_name" {
  description = "Name of the PBS authorization V2 table"
  type        = string
}

variable "budget_key_table_name" {
  description = "Name of the budget key table"
  type        = string
}

variable "partition_lock_table_name" {
  description = "Name of the partition lock table"
  type        = string
}

variable "auth_table_read_max_capacity" {
  type        = number
  description = "Max read capacity for auth table"

}

variable "auth_table_write_max_capacity" {
  type        = number
  description = "Max write capacity for auth table"

}

variable "pbs_authorization_v2_table_read_max_capacity" {
  type        = number
  description = "Max read capacity for auth table"

}

variable "pbs_authorization_v2_table_write_max_capacity" {
  type        = number
  description = "Max write capacity for auth table"

}

variable "budget_table_read_capacity" {
  description = "Max read capacity for budget table"
  type        = number
}

variable "budget_table_write_capacity" {
  description = "Max write capacity for budget table"
  type        = number
}

variable "partition_lock_table_read_capacity" {
  description = "Max read capacity for partition lock table"
  type        = number
}

variable "partition_lock_table_write_capacity" {
  description = "Max write capacity for partition lock table"
  type        = number
}

variable "budget_key_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of budget key table read processing unit"
  type        = string
}

variable "budget_key_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of budget key table write processing unit"
  type        = string
}

variable "reporting_origin_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of reporting origin table read processing unit"
  type        = string
}

variable "reporting_origin_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of reporting origin table write processing unit"
  type        = string
}

variable "pbs_authorization_v2_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of PBS authorization V2 table read processing unit"
  type        = string
}

variable "pbs_authorization_v2_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of PBS authorization V2 table write processing unit"
  type        = string
}

variable "partition_lock_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of partition lock key table read processing unit"
  type        = string
}

variable "partition_lock_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of partition lock key table write processing unit"
  type        = string
}
