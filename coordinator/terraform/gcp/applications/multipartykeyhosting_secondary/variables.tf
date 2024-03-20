################################################################################
# Global Variables.
################################################################################

variable "project_id" {
  description = "GCP Project ID in which this module will be created."
  type        = string
}

variable "wipp_project_id_override" {
  description = "Override the GCP Project ID in which workload identity pool (wip) and workload identity pool provider (wipp) will be created in."
  type        = string
}

variable "wip_allowed_service_account_project_id_override" {
  description = "Override the GCP Project ID in which wip allowed service account will be created in."
  type        = string
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "primary_region" {
  description = "Region where all services will be created."
  type        = string
}

variable "primary_region_zone" {
  description = "Region zone where all services will be created."
  type        = string
}

variable "secondary_region" {
  description = "Region where all services will be replicated."
  type        = string
}

variable "secondary_region_zone" {
  description = "Region zone where all services will be replicated."
  type        = string
}

variable "mpkhs_secondary_package_bucket_location" {
  description = "Location for multiparty keyhosting packages. Example: 'US'."
  type        = string
}

variable "enable_audit_log" {
  description = "Enable Data Access Audit Logging for projects containing the WIP and Service Accounts."
  type        = bool
}

################################################################################
# Global Alarm Variables.
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for mpkhs services."
  type        = bool
}

variable "alarms_notification_email" {
  description = "Email to receive alarms for mpkhs services."
  type        = string
}

################################################################################
# Spanner Variables.
################################################################################

variable "spanner_instance_config" {
  description = "Config value for the Spanner Instance."
  type        = string
}

variable "spanner_processing_units" {
  description = "Spanner's compute capacity. 1000 processing units = 1 node and must be set as a multiple of 100."
  type        = number
}

# Similar to Point-in-time recovery for AWS DynamoDB
# Must be between 1 hour and 7 days. Can be specified in days, hours, minutes, or seconds.
# eg: 1d, 24h, 1440m, and 86400s are equivalent.
variable "key_db_retention_period" {
  description = "Duration to maintain table versioning for point-in-time recovery."
  type        = string
  nullable    = false
}

################################################################################
# Routing Variables.
################################################################################

variable "enable_domain_management" {
  description = "Manage domain SSL cert creation and routing for encryption key and key storage services."
  type        = bool
}

variable "parent_domain_name" {
  description = "Custom domain name to use with key hosting APIs."
  type        = string
}

variable "parent_domain_name_project" {
  description = "Project ID where custom domain name hosted zone is located."
  type        = string
}

variable "service_subdomain_suffix" {
  description = "When set, the value replaces `-$${var.environment}` as the service subdomain suffix."
  type        = string
}

variable "encryption_key_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the encryption key service."
  type        = string
}

variable "key_storage_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the key storage service."
  type        = string
}

################################################################################
# Cloud Function Variables.
################################################################################

variable "cloudfunction_timeout_seconds" {
  description = "Number of seconds after which a function instance times out."
  type        = number
}

variable "encryption_key_service_zip" {
  description = <<-EOT
          Encryption key service cloud function path. If not provided defaults to locally built zip file.
        Build with `bazel build //coordinator/terraform/gcp/applications/multipartykeyhosting_secondary:all`.
      EOT
  type        = string
}

variable "encryption_key_service_cloudfunction_memory_mb" {
  description = "Memory size in MB for encryption key cloud function."
  type        = number
}

variable "encryption_key_service_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
}

variable "encryption_key_service_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
}

variable "key_storage_service_zip" {
  description = <<-EOT
          Key storage service cloud function path. If not provided defaults to locally built jar file.
        Build with `bazel build //coordinator/terraform/gcp/applications/multipartykeyhosting_secondary:all`.
      EOT
  type        = string
}

variable "key_storage_service_cloudfunction_memory_mb" {
  description = "Memory size in MB for key storage cloud function."
  type        = number
}

variable "key_storage_service_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
}

variable "key_storage_service_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
}

################################################################################
# Key Management Variables.
################################################################################

# TODO: Manage group in terraform
variable "allowed_wip_user_group" {
  description = "Google Group to manage allowed coordinator users."
  type        = string
}

variable "allowed_operator_user_group" {
  description = "Google group of allowed operators to which to give API access."
  type        = string
}

variable "enable_attestation" {
  description = "Enable attestation requirement for this Workload Identity Pool. Assertion TEE vars required."
  type        = bool
}

variable "assertion_tee_swname" {
  description = "Software Running TEE. Example: 'CONFIDENTIAL_SPACE'."
  type        = string
}

variable "assertion_tee_support_attributes" {
  description = "Required support attributes of the Confidential Space image. Example: \"STABLE\", \"LATEST\". Empty for debug images."
  type        = list(string)
}

variable "assertion_tee_container_image_reference_list" {
  description = "List of acceptable image names TEE can run."
  type        = list(string)
}

variable "assertion_tee_container_image_hash_list" {
  description = "List of acceptable binary hashes TEE can run."
  type        = list(string)
}

################################################################################
# Key Storage Alarm Variables.
################################################################################

variable "keystorageservice_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "keystorageservice_cloudfunction_error_threshold" {
  description = "Error count greater than this to send alarm. Example: 0."
  type        = string
}

variable "keystorageservice_cloudfunction_max_execution_time_max" {
  description = "Max execution time in ms to send alarm. Example: 9999."
  type        = string
}

variable "keystorageservice_cloudfunction_5xx_threshold" {
  description = "Cloud Function 5xx error count greater than this to send alarm. Example: 0."
  type        = string
}

variable "keystorageservice_lb_max_latency_ms" {
  description = "Load Balancer max latency to send alarm. Measured in milliseconds. Example: 5000."
  type        = string
}

variable "keystorageservice_lb_5xx_threshold" {
  description = "Load Balancer 5xx error count greater than this to send alarm. Example: 0."
  type        = string
}

variable "keystorageservice_alarm_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = number
}

################################################################################
# Encryption Key Service Alarm Variables.
################################################################################

variable "encryptionkeyservice_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "encryptionkeyservice_cloudfunction_error_threshold" {
  description = "Error count greater than this to send alarm. Example: 0."
  type        = string
}

variable "encryptionkeyservice_cloudfunction_max_execution_time_max" {
  description = "Max execution time in ms to send alarm. Example: 9999."
  type        = string
}

variable "encryptionkeyservice_cloudfunction_5xx_threshold" {
  description = "Cloud Function 5xx error count greater than this to send alarm. Example: 0."
  type        = string
}

variable "encryptionkeyservice_lb_max_latency_ms" {
  description = "Load Balancer max latency to send alarm. Measured in milliseconds. Example: 5000."
  type        = string
}

variable "encryptionkeyservice_lb_5xx_threshold" {
  description = "Load Balancer 5xx error count greater than this to send alarm. Example: 0."
  type        = string
}

variable "encryptionkeyservice_alarm_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = number
}
