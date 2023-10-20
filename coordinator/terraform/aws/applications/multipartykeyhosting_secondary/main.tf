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

#Default provider
provider "aws" {
  region = var.region
}

locals {
  create_encryption_key_jar          = var.create_encryption_key_jar != "" ? var.create_encryption_key_jar : "${module.bazel.bazel_bin}/java/com/google/scp/coordinator/keymanagement/keystorage/service/aws/KeyStorageServiceLambda_deploy.jar"
  encryption_key_service_jar         = var.encryption_key_service_jar != "" ? var.encryption_key_service_jar : "${module.bazel.bazel_bin}/java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws/EncryptionKeyServiceLambda_deploy.jar"
  use_sns_to_sqs                     = var.alarms_enabled && var.sns_topic_arn != "" && var.sqs_queue_arn != ""
  keydb_table_name                   = var.keydb_table_name != "" ? var.keydb_table_name : "${var.environment}_keydb"
  keyhosting_api_gateway_name        = "unified_key_hosting"
  keyhosting_api_gateway_alarm_label = "UnifiedKeyHosting"
  keystorage_api_gateway_alarm_label = "KeyStorage"
  keystorage_api_gateway_name        = "key_storage"
  account_id                         = data.aws_caller_identity.current.account_id
}

# Needed for dashboard ARNs
data "aws_caller_identity" "current" {}

# Hosted zone must exist with CAA records for domain and wildcard subdomain
# See /applications/domainrecords for an example
data "aws_route53_zone" "hosted_zone" {
  count        = var.enable_domain_management ? 1 : 0
  name         = var.parent_domain_name
  private_zone = false
}

module "bazel" {
  source = "../../modules/bazel"
}

module "keymanagementpackagebucket_primary" {
  source      = "../../modules/shared/bucketprovider"
  environment = var.environment
  name_suffix = "jars"
}

module "keydb" {
  source = "../../modules/keydb"

  environment = var.environment
  table_name  = local.keydb_table_name
  # No fallback DDB region for Coordinator B deployments.
  keydb_replica_regions = []

  enable_dynamo_point_in_time_recovery = var.enable_dynamo_point_in_time_recovery
  enable_keydb_ttl                     = var.enable_keydb_ttl

  read_capacity                       = var.keydb_read_capacity
  autoscaling_read_max_capacity       = var.keydb_autoscaling_read_max_capacity
  autoscaling_read_target_percentage  = var.keydb_autoscaling_read_target_percentage
  write_capacity                      = var.keydb_write_capacity
  autoscaling_write_max_capacity      = var.keydb_autoscaling_write_max_capacity
  autoscaling_write_target_percentage = var.keydb_autoscaling_write_target_percentage
}

module "vpc" {
  source = "../../modules/vpc_v2"
  count  = var.enable_vpc ? 1 : 0

  environment             = var.environment
  enable_dynamodb_replica = false
  dynamodb_arn            = module.keydb.key_db_arn

  # Coordinator B does not need internet access for any of its Lambdas
  create_public_subnet         = false
  vpc_cidr                     = var.vpc_cidr
  vpc_default_tags             = var.vpc_default_tags
  availability_zone_replicas   = var.availability_zone_replicas
  enable_dynamodb_vpc_endpoint = true

  # Because the secondary coordinator only supports one region, force enable VPC
  # regardless of whether dynamodb replica is enabled.
  force_enable = true

  lambda_execution_role_ids = [
    module.encryptionkeyservice.lambda_execution_role_id,
    module.keystorageservice.lambda_execution_role_id
  ]

  dynamodb_allowed_principal_arns = [
    module.encryptionkeyservice.lambda_principal_role_arn,
    module.keystorageservice.lambda_principal_role_arn
  ]

  enable_kms_vpc_endpoint = true
  kms_keys_arns = compact([
    module.keystorageservice.kms_key_arn,
    module.keystorageservice.data_key_signature_key_arn,
    module.keystorageservice.data_key_encryptor_arn,
    # "compact" will omit this if empty due to signatures being disabled.
    module.keystorageservice.encryption_key_signature_key_arn,
  ])
  kms_allowed_principal_arns = [
    module.keystorageservice.lambda_principal_role_arn
  ]

  enable_flow_logs            = var.enable_vpc_flow_logs
  flow_logs_traffic_type      = var.vpc_flow_logs_traffic_type
  flow_logs_retention_in_days = var.vpc_flow_logs_retention_in_days
}

module "keystorageservice" {
  source = "../../modules/keystorageservice"

  keymanagement_package_bucket = module.keymanagementpackagebucket_primary.bucket_id
  key_storage_jar              = local.create_encryption_key_jar

  environment = var.environment

  dynamo_keydb = {
    table_name = local.keydb_table_name
    arn        = module.keydb.key_db_arn
    region     = var.region
  }

  create_key_lambda_memory_mb       = 2048
  create_key_logging_retention_days = var.cloudwatch_logging_retention_days

  get_data_key_lambda_memory_mb       = 2048
  get_data_key_logging_retention_days = var.cloudwatch_logging_retention_days

  allowed_peer_coordinator_principals      = var.allowed_peer_coordinator_principals
  keygeneration_attestation_condition_keys = var.keygeneration_attestation_condition_keys
  enable_public_key_signature              = var.enable_public_key_signature

  #API Gateway vars
  create_key_lambda_logging_retention_days   = var.cloudwatch_logging_retention_days
  get_data_key_lambda_logging_retention_days = var.cloudwatch_logging_retention_days
  api_gateway_execution_arn                  = module.keystorageapigateway.api_gateway_execution_arn
  api_gateway_id                             = module.keystorageapigateway.api_gateway_id
  api_version                                = var.api_version

  #Alarms
  key_storage_service_alarms_enabled       = var.alarms_enabled
  create_key_sns_topic_arn                 = var.alarms_enabled ? (var.sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.sns_topic_arn) : null
  create_key_alarm_eval_period_sec         = var.mpkhs_alarm_eval_period_sec
  create_key_lambda_error_threshold        = var.mpkhs_lambda_error_threshold
  create_key_lambda_error_log_threshold    = var.mpkhs_lambda_error_log_threshold
  create_key_lambda_max_duration_threshold = var.mpkhs_lambda_max_duration_threshold

  get_data_key_sns_topic_arn                 = var.alarms_enabled ? (var.sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.sns_topic_arn) : null
  get_data_key_alarm_eval_period_sec         = var.mpkhs_alarm_eval_period_sec
  get_data_key_lambda_error_threshold        = var.mpkhs_lambda_error_threshold
  get_data_key_lambda_error_log_threshold    = var.mpkhs_lambda_error_log_threshold
  get_data_key_lambda_max_duration_threshold = var.mpkhs_lambda_max_duration_threshold

  enable_vpc               = var.enable_vpc
  lambda_sg_ids            = var.enable_vpc ? [module.vpc[0].allow_internal_ingress_sg_id, module.vpc[0].allow_egress_sg_id] : null
  dynamodb_vpc_endpoint_id = var.enable_vpc ? module.vpc[0].dynamodb_vpc_endpoint_id : null
  private_subnet_ids       = var.enable_vpc ? module.vpc[0].private_subnet_ids : null
  kms_vpc_endpoint_id      = var.enable_vpc ? module.vpc[0].kms_vpc_endpoint_id : null
  custom_alarm_label       = var.custom_alarm_label
}

# Topic is used for alarms such that messages are not sensitive data.
#tfsec:ignore:aws-sns-enable-topic-encryption
resource "aws_sns_topic" "mpkhs" {
  count = var.alarms_enabled ? 1 : 0
  name  = "${var.environment}_mpkhs_sns_topic"
  tags = {
    environment = var.environment
  }
}

resource "aws_sns_topic_subscription" "mpkhs" {
  count     = var.alarms_enabled ? 1 : 0
  topic_arn = local.use_sns_to_sqs ? var.sns_topic_arn : aws_sns_topic.mpkhs[0].arn
  protocol  = local.use_sns_to_sqs ? "sqs" : "email"
  endpoint  = local.use_sns_to_sqs ? var.sqs_queue_arn : var.alarm_notification_email
}

module "encryptionkeyservice" {
  source                     = "../../modules/encryptionkeyservice"
  encryption_key_service_jar = local.encryption_key_service_jar

  environment = var.environment

  dynamo_keydb = {
    name   = local.keydb_table_name
    arn    = module.keydb.key_db_arn
    region = var.region
  }

  keymanagement_package_bucket = module.keymanagementpackagebucket_primary.bucket_id
  api_version                  = var.api_version

  logging_retention_days = var.cloudwatch_logging_retention_days
  #Alarms
  alarms_enabled                = var.alarms_enabled
  sns_topic_arn                 = var.alarms_enabled ? (var.sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.sns_topic_arn) : null
  alarm_eval_period_sec         = var.mpkhs_alarm_eval_period_sec
  lambda_error_threshold        = var.mpkhs_lambda_error_threshold
  lambda_error_log_threshold    = var.mpkhs_lambda_error_log_threshold
  lambda_max_duration_threshold = var.mpkhs_lambda_max_duration_threshold

  get_encryption_key_lambda_ps_client_shim_enabled = var.get_encryption_key_lambda_ps_client_shim_enabled

  api_gateway_id            = module.keyhostingapigateway.api_gateway_id
  api_gateway_execution_arn = module.keyhostingapigateway.api_gateway_execution_arn

  enable_vpc               = var.enable_vpc
  lambda_sg_ids            = var.enable_vpc ? [module.vpc[0].allow_internal_ingress_sg_id, module.vpc[0].allow_egress_sg_id] : null
  dynamodb_vpc_endpoint_id = var.enable_vpc ? module.vpc[0].dynamodb_vpc_endpoint_id : null
  private_subnet_ids       = var.enable_vpc ? module.vpc[0].private_subnet_ids : null
  custom_alarm_label       = var.custom_alarm_label
}

module "privatekeydomainprovider" {
  source = "../../modules/privatekeydomainprovider"
  count  = var.enable_domain_management ? 1 : 0

  environment = var.environment

  parent_domain_name = var.parent_domain_name

  domain_hosted_zone_id                = data.aws_route53_zone.hosted_zone[0].zone_id
  get_private_key_api_gateway_id       = module.keyhostingapigateway.api_gateway_id
  get_private_key_api_gateway_stage_id = module.keyhostingapigateway.gateway_stage_id
  service_subdomain                    = var.encryption_key_service_subdomain
}

module "keyhostingapigateway" {
  source = "../../modules/apigateway"

  api_env_stage_name = var.api_env_stage_name
  environment        = var.environment
  name               = local.keyhosting_api_gateway_alarm_label

  api_gateway_logging_retention_days = var.cloudwatch_logging_retention_days
  #Alarms
  api_gateway_alarms_enabled        = var.alarms_enabled
  api_gateway_5xx_threshold         = var.mpkhs_api_gw_5xx_threshold
  api_gateway_alarm_eval_period_sec = var.mpkhs_alarm_eval_period_sec
  api_gateway_api_max_latency_ms    = var.mpkhs_api_gw_max_latency_ms
  api_gateway_sns_topic_arn         = var.alarms_enabled ? (var.sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.sns_topic_arn) : null
  custom_alarm_label                = var.custom_alarm_label
}

module "keystorageapigateway" {
  source = "../../modules/apigateway"

  api_env_stage_name = var.api_env_stage_name
  environment        = var.environment
  name               = local.keystorage_api_gateway_alarm_label

  api_gateway_logging_retention_days = var.cloudwatch_logging_retention_days
  #Alarms
  api_gateway_alarms_enabled        = var.alarms_enabled
  api_gateway_5xx_threshold         = var.mpkhs_api_gw_5xx_threshold
  api_gateway_alarm_eval_period_sec = var.mpkhs_alarm_eval_period_sec
  api_gateway_api_max_latency_ms    = var.mpkhs_api_gw_max_latency_ms
  api_gateway_sns_topic_arn         = var.alarms_enabled ? (var.sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.sns_topic_arn) : null
  custom_alarm_label                = var.custom_alarm_label
}

module "keystoragedashboard" {
  source = "../../modules/keystoragedashboard"
  count  = var.alarms_enabled ? 1 : 0

  account_id                 = local.account_id
  environment                = var.environment
  region                     = var.region
  key_storage_api_gateway_id = module.keystorageapigateway.api_gateway_id

  create_key_lambda_version   = module.keystorageservice.create_key_lambda_version
  get_data_key_lambda_version = module.keystorageservice.get_data_key_lambda_version

  # Re-using the existing unified_keyhosting_dashboard flag.
  key_storage_dashboard_time_period_seconds = var.unified_keyhosting_dashboard_time_period_seconds
}

module "unifiedkeyhostingdashboard" {
  source = "../../modules/unifiedkeyhostingdashboard"
  count  = var.alarms_enabled ? 1 : 0

  account_id  = local.account_id
  environment = var.environment
  region      = var.region

  get_encryption_key_lambda_version = module.encryptionkeyservice.lambda_version

  unified_keyhosting_api_gateway_id                = module.keyhostingapigateway.api_gateway_id
  unified_keyhosting_dashboard_time_period_seconds = var.unified_keyhosting_dashboard_time_period_seconds
}
