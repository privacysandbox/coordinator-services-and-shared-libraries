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
# Global Variables. Default values defined in shared/mpkhs/mpkhs_variables.tf
################################################################################

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "primary_region" {
  description = "Region where all services will be created. Serves as primary region for keydb and primary origin for cloudfront."
  type        = string
}

variable "secondary_region" {
  description = "Region where public keyhosting service is replicated. Serves as failover region for cloudfront."
  type        = string
}

################################################################################
# DynamoDB Variables.
################################################################################

variable "keydb_table_name" {
  description = "Name of the DynamoDB table. Defaults to {var.environment}_keydb if unspecified."
  type        = string
}

# DynamoDB table cannot be created at the same time as replica because of AWS SDK limitations
# It must be set to false on initial deployment
# https://github.com/hashicorp/terraform-provider-aws/issues/13097#issuecomment-933806064
variable "enable_dynamodb_replica" {
  description = "Enable creation of dynamodb replica."
  type        = bool
}

variable "enable_dynamo_point_in_time_recovery" {
  description = "Allows DynamoDB table data to be recovered after table deletion."
  type        = bool
}

variable "enable_keydb_ttl" {
  description = "Allows DynamoDB table data to delete keys automatically based on the TtlTime attribute."
  type        = bool
}

variable "keydb_write_capacity" {
  description = "Provisioned write capacity units."
  type        = number
}

variable "keydb_read_capacity" {
  description = "Provisioned read capacity units."
  type        = number
}

variable "keydb_autoscaling_read_max_capacity" {
  description = "Maximum read capacity units."
  type        = number
}

variable "keydb_autoscaling_read_target_percentage" {
  description = "Read capacity target utilization."
  type        = number
}

variable "keydb_autoscaling_write_max_capacity" {
  description = "Maximum write capacity units."
  type        = number
}

variable "keydb_autoscaling_write_target_percentage" {
  description = "Write capacity target utilization."
  type        = number
}

################################################################################
# Key Management Variables.
################################################################################

variable "key_generation_ami_name_prefix" {
  description = "AMI name prefix for Key Generation Enclave AMI"
  type        = string
}

variable "key_generation_ami_owners" {
  description = "AMI owner list used to search for for $key_generation_ami_name_prefix"
  type        = list(string)
  # See https://registry.terraform.io/providers/hashicorp/aws/latest/docs/data-sources/ami#owners
}

variable "keystorage_api_base_url" {
  description = "Base Url of Key Storage Api, including api stage and version but no trailing slash."
  type        = string
}

variable "key_generation_frequency_in_hours" {
  description = "Cron frequency in hours between of key generation invocations."
  type        = number
}

variable "key_generation_count" {
  description = "Number of keys to generate at a time."
  type        = number
}

variable "key_generation_validity_in_days" {
  description = "Number of days keys will be valid. Should be greater than generation days for failover validity."
  type        = number
}

variable "key_generation_ttl_in_days" {
  description = "Keys will be deleted from the database this number of days after creation time."
  type        = number
}

variable "get_public_key_cache_control_max_seconds" {
  description = "Value of Cache-Control max-age API header in seconds. Should reflect the key generation rate."
  type        = number
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

variable "coordinator_b_assume_role_arn" {
  description = <<-EOT
     Role to assume when interacting with Coordinator B services. If empty, no
     parameter is created and the instance's credentials are used.
  EOT
  type        = string
}

variable "enable_public_key_signature" {
  description = "Generate a public key signature key for this coordinator and include signatures."
  type        = bool
}

variable "key_id_type" {
  description = "Key ID Type"
  type        = string
}

################################################################################
# Lambda Variables.
################################################################################

variable "public_key_service_jar" {
  description = <<-EOT
          Public key service lambda path. If not provided defaults to locally built jar file.
        Build with `bazel build //coordinator/terraform/aws/modules/singlepartykeymanagement:all`.
      EOT
  type        = string
}

variable "encryption_key_service_jar" {
  description = <<-EOT
        Private key service lambda path. If not provided defaults to locally built jar file.
        Build with `bazel build //coordinator/terraform/aws/modules/singlepartykeymanagement:all`.
      EOT
  type        = string
}

variable "get_public_key_lambda_memory_mb" {
  description = "Memory size in MB for lambda function."
  type        = number
}

variable "get_public_key_lambda_provisioned_concurrency_enabled" {
  description = "Whether to use use provisioned concurrency for get public key lambda."
  type        = bool
}

variable "get_public_key_lambda_provisioned_concurrency_count" {
  description = "Number of lambda instances to initialize for provisioned concurrency."
  type        = number
}

variable "get_encryption_key_lambda_ps_client_shim_enabled" {
  description = <<-EOT
        If true, enables shim to ensure response is compatible with privacy sandbox 0.51.x
	clients. Newer fields such as activationTime is filtered out from the response.
	This temporary variable will be removed in the future.
      EOT
  type        = bool
}

################################################################################
# Routing Variables.
################################################################################

variable "enable_domain_management" {
  description = "Manage domain SSL cert creation and routing for public and private key services."
  type        = bool
}

variable "parent_domain_name" {
  description = <<-EOT
    Custom domain name to register and use with key management APIs.
    Default to empty string so it does not have to be populated when enable_domain_management = false".
  EOT
  type        = string
}

variable "public_key_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the public key service."
  type        = string
}

variable "public_key_service_alternate_domain_names" {
  description = "A list of alternate domain names for the public key service for which to add as a subject alternative name in the SSL certificate. Eg: [service-example.com]"
  type        = list(string)
}

variable "public_key_service_domain_name_to_domain_hosted_zone_id" {
  description = "(Optiontal) a Map of domain_names to the hosted zone id it belongs to that should be used to verify the SANs."
  type        = map(string)
}

variable "encryption_key_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the private key service."
  type        = string
}

variable "api_version" {
  description = "Version of the created APIs. Eg: v1."
  type        = string
}

variable "api_env_stage_name" {
  description = "Stage name for API gateway instances."
  type        = string
}

variable "application_name" {
  description = "Application name that provide public key service. Eg: aggregation-service."
  type        = string
}

################################################################################
# Global Alarm Variables.
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for mpkhs services."
  type        = bool
}

variable "alarm_notification_email" {
  description = "Email to receive alarms for public key services."
  type        = string
}

variable "cloudwatch_logging_retention_days" {
  description = "Number of days to keep logs in Cloudwatch."
  type        = number
}

variable "primary_region_sns_topic_arn" {
  description = "SNS topic ARN to forward alerts to in the primary region"
  type        = string
}

variable "primary_region_sqs_queue_arn" {
  description = "SQS queue ARN to forward alerts to in the primary region"
  type        = string
}

variable "secondary_region_sns_topic_arn" {
  description = "SNS topic ARN to forward alerts to in the secondary region"
  type        = string
}

variable "secondary_region_sqs_queue_arn" {
  description = "SQS queue ARN to forward alerts to in the secondary region"
  type        = string
}

################################################################################
# Shared Alarm Variables.
################################################################################

variable "mpkhs_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
  default     = "300"
}

variable "mpkhs_lambda_error_threshold" {
  description = "Error rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
  default     = "0.0"
}

variable "mpkhs_lambda_error_log_threshold" {
  description = "Error log sum greater than this to send alarm. Example: '0'."
  type        = string
  default     = "0"
}

variable "mpkhs_lambda_max_duration_threshold" {
  description = "Lambda max duration in ms to send alarm. Useful for timeouts. Default 9999ms since lambda time out is set to 10s."
  type        = string
  default     = "9999"
}

variable "mpkhs_api_gw_max_latency_ms" {
  description = "API Gateway max latency to send alarm. Measured in milliseconds. Default 10s because API takes 6-7 seconds on cold start."
  type        = string
  default     = "10000"
}

variable "mpkhs_api_gw_5xx_threshold" {
  description = "API Gateway 5xx error rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
  default     = "0.0"
}

variable "mpkhs_api_gw_error_ratio_4xx_threshold" {
  description = "API Gateway 4xx error ratio rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

variable "mpkhs_api_gw_error_ratio_5xx_threshold" {
  description = "API Gateway 5xx error ratio rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

variable "mpkhs_total_queue_messages_high_threshold" {
  description = "Total Queue Messages greater than this to send alarm. Must be in integer form: 10 = 10. Example: '2'."
  type        = string
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}

################################################################################
# Cloudfront Alarm Variables.
################################################################################

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

################################################################################
# VPC Variables.
################################################################################

variable "enable_vpc" {
  description = "Enable the creation of customer-owned VPCs providing network security."
  type        = bool
}

variable "vpc_cidr" {
  description = "VPC CIDR range for secure VPC."
  type        = string
}

variable "availability_zone_replicas" {
  description = "Number of subnet groups to create over additional availability zones in the region (up to 4 for /16, if supported by AWS region)."
  type        = number
}

variable "vpc_default_tags" {
  description = "Tags to add to the secure VPC network infrastructure."
  type        = map(string)
}

variable "enable_vpc_flow_logs" {
  description = "Whether to enable VPC flow logs that end up in a cloudwatch log group."
  type        = bool
}

variable "vpc_flow_logs_traffic_type" {
  description = "The traffic type to log. Either ACCEPT, REJECT or ALL."
  type        = string
}

variable "vpc_flow_logs_retention_in_days" {
  description = "The number of days to keep the flow logs in CloudWatch."
  type        = number
}

################################################################################
# Dashboard Variables.
################################################################################

variable "unified_keyhosting_dashboard_time_period_seconds" {
  description = "Time period that acts as window for dashboard metrics. Measured in seconds."
  type        = number
}

variable "public_keyhosting_dashboard_time_period_seconds" {
  description = "Time period that acts as window for dashboard metrics. Measured in seconds."
  type        = number
}
