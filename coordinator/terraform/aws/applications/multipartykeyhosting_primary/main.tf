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

# Default provider
provider "aws" {
  region = var.primary_region
}

provider "aws" {
  alias  = "secondary"
  region = var.secondary_region
}

provider "aws" {
  alias  = "useast1"
  region = "us-east-1"
}

locals {
  public_key_service_jar     = var.public_key_service_jar != "" ? var.public_key_service_jar : "${module.bazel.bazel_bin}/java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws/PublicKeyApiGatewayHandlerLambda_deploy.jar"
  encryption_key_service_jar = var.encryption_key_service_jar != "" ? var.encryption_key_service_jar : "${module.bazel.bazel_bin}/java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws/EncryptionKeyServiceLambda_deploy.jar"
  use_sns_to_sqs_primary     = var.alarms_enabled && var.primary_region_sns_topic_arn != "" && var.primary_region_sqs_queue_arn != ""
  use_sns_to_sqs_secondary   = var.alarms_enabled && var.secondary_region_sns_topic_arn != "" && var.secondary_region_sqs_queue_arn != ""
  # Always must be us-east-1 -- either the primary or secondary is us-east-1
  sns_cloudfront_arn                 = var.primary_region == "us-east-1" ? var.primary_region_sns_topic_arn : var.secondary_region_sns_topic_arn
  sqs_cloudfront_arn                 = var.primary_region == "us-east-1" ? var.primary_region_sqs_queue_arn : var.secondary_region_sqs_queue_arn
  use_cloudfront_arn                 = local.sns_cloudfront_arn != "" && local.sqs_cloudfront_arn != ""
  keydb_table_name                   = var.keydb_table_name != "" ? var.keydb_table_name : "${var.environment}_keydb"
  keyhosting_api_gateway_name        = "unified_key_hosting"
  keyhosting_api_gateway_alarm_label = "UnifiedKeyHosting"
  key_rotation_job_queue_name        = "key-rotation-queue.fifo"
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

module "keymanagementpackagebucket_secondary" {
  source = "../../modules/shared/bucketprovider"

  providers = {
    aws = aws.secondary
  }

  environment = var.environment
  name_suffix = "jars"
}

module "key_job_queue" {
  source = "../../modules/keyjobqueue"

  environment        = var.environment
  key_job_queue_name = local.key_rotation_job_queue_name
}

module "vpc" {
  source = "../../modules/vpc_v2"
  count  = var.enable_vpc ? 1 : 0

  environment                  = var.environment
  force_enable                 = false
  create_public_subnet         = true # key gen enclave requires outbound internet access
  vpc_cidr                     = var.vpc_cidr
  vpc_default_tags             = var.vpc_default_tags
  availability_zone_replicas   = var.availability_zone_replicas
  enable_dynamodb_vpc_endpoint = true
  enable_dynamodb_replica      = var.enable_dynamodb_replica
  dynamodb_arn                 = module.keydb.key_db_arn

  lambda_execution_role_ids = [
    module.publickeyhostingservice_primary.get_public_key_lambda_principal_role_id,
    module.encryptionkeyservice.lambda_execution_role_id,
  ]

  dynamodb_allowed_principal_arns = [
    module.split_key_generation_service.key_generation_enclave_role_arn,
    module.publickeyhostingservice_primary.get_public_key_lambda_principal_role_arn,
    module.encryptionkeyservice.lambda_principal_role_arn,
  ]

  # TODO(b/243725548): Traffic to services such as KMS, SQS and SSM are
  # accessed thru the public internet instead of the VPC endpoints.
  enable_kms_vpc_endpoint    = false
  kms_keys_arns              = []
  kms_allowed_principal_arns = []

  enable_flow_logs            = var.enable_vpc_flow_logs
  flow_logs_traffic_type      = var.vpc_flow_logs_traffic_type
  flow_logs_retention_in_days = var.vpc_flow_logs_retention_in_days
}

module "vpc_secondary" {
  source = "../../modules/vpc_v2"
  count  = var.enable_vpc ? 1 : 0

  providers = {
    aws = aws.secondary
  }

  environment                  = var.environment
  force_enable                 = false
  create_public_subnet         = false
  vpc_cidr                     = var.vpc_cidr
  vpc_default_tags             = var.vpc_default_tags
  availability_zone_replicas   = var.availability_zone_replicas
  enable_dynamodb_vpc_endpoint = true
  enable_dynamodb_replica      = var.enable_dynamodb_replica
  dynamodb_arn                 = var.enable_dynamodb_replica ? replace(module.keydb.key_db_arn, var.primary_region, var.secondary_region) : null

  lambda_execution_role_ids = [
    module.publickeyhostingservice_secondary.get_public_key_lambda_principal_role_id
  ]

  dynamodb_allowed_principal_arns = [
    module.publickeyhostingservice_secondary.get_public_key_lambda_principal_role_arn
  ]

  # KMS VPC endpoint is not necessary in the secondary region, because the keygen
  # enclave only needs to access KMS in the primary region
  enable_kms_vpc_endpoint    = false
  kms_keys_arns              = []
  kms_allowed_principal_arns = []

  enable_flow_logs            = var.enable_vpc_flow_logs
  flow_logs_traffic_type      = var.vpc_flow_logs_traffic_type
  flow_logs_retention_in_days = var.vpc_flow_logs_retention_in_days
}

module "split_key_generation_service" {
  source      = "../../modules/multipartykeygenerationservice"
  environment = var.environment

  ami_name                = var.key_generation_ami_name_prefix
  ami_owners              = var.key_generation_ami_owners
  keystorage_api_base_url = var.keystorage_api_base_url
  keyjobqueue_url         = module.key_job_queue.keyjobqueue_url
  keyjobqueue_arn         = module.key_job_queue.keyjobqueue_arn
  keydb_table_name        = local.keydb_table_name
  keydb_region            = var.primary_region
  keydb_arn               = module.keydb.key_db_arn
  key_id_type             = var.key_id_type

  coordinator_b_assume_role_arn = var.coordinator_b_assume_role_arn
  enable_public_key_signature   = var.enable_public_key_signature

  # Secure VPC variables
  enable_vpc               = var.enable_vpc
  secure_vpc_sg_ids        = var.enable_vpc ? [module.vpc[0].allow_internal_ingress_sg_id, module.vpc[0].allow_egress_sg_id] : []
  secure_vpc_subnet_ids    = var.enable_vpc ? module.vpc[0].private_subnet_ids : []
  dynamodb_vpc_endpoint_id = var.enable_vpc ? module.vpc[0].dynamodb_vpc_endpoint_id : ""
}

# Topic is used for alarms such that messages are not sensitive data.
#tfsec:ignore:aws-sns-enable-topic-encryption
resource "aws_sns_topic" "mpkhs" {
  count = var.alarms_enabled ? 1 : 0
  name  = "${var.environment}_${var.primary_region}_mpkhs_sns_topic"

  tags = {
    environment = var.environment
  }
}

resource "aws_sns_topic_subscription" "mpkhs" {
  count     = var.alarms_enabled ? 1 : 0
  topic_arn = local.use_sns_to_sqs_primary ? var.primary_region_sns_topic_arn : aws_sns_topic.mpkhs[0].arn
  protocol  = local.use_sns_to_sqs_primary ? "sqs" : "email"
  endpoint  = local.use_sns_to_sqs_primary ? var.primary_region_sqs_queue_arn : var.alarm_notification_email
}

# Topic is used for alarms such that messages are not sensitive data.
#tfsec:ignore:aws-sns-enable-topic-encryption
resource "aws_sns_topic" "mpkhs_snstopic_secondary" {
  count    = var.alarms_enabled ? 1 : 0
  provider = aws.secondary
  name     = "${var.environment}_${var.secondary_region}_mpkhs_sns_topic"

  tags = {
    environment = var.environment
  }
}

resource "aws_sns_topic_subscription" "mpkhs_snstopic_secondary_email_subscription" {
  count    = var.alarms_enabled ? 1 : 0
  provider = aws.secondary

  topic_arn = local.use_sns_to_sqs_secondary ? var.secondary_region_sns_topic_arn : aws_sns_topic.mpkhs_snstopic_secondary[0].arn

  #Email requires confirmation
  protocol = local.use_sns_to_sqs_secondary ? "sqs" : "email"
  endpoint = local.use_sns_to_sqs_secondary ? var.secondary_region_sqs_queue_arn : var.alarm_notification_email
}

# Resource is created only if us-east-1 is not primary or secondary region.
# In this case, however, neither terraform nor AWS warn about the clashing names, and an extra topic is not created.
# This resource refers to the topic in us-east-1 nonetheless.
# Topic is used for alarms such that messages are not sensitive data.
#tfsec:ignore:aws-sns-enable-topic-encryption
resource "aws_sns_topic" "mpkhs_snstopic_useast1" {
  count    = var.alarms_enabled ? 1 : 0
  provider = aws.useast1
  name     = "${var.environment}_us-east-1_mpkhs_sns_topic"

  tags = {
    environment = var.environment
  }
}

resource "aws_sns_topic_subscription" "mpkhs_snstopic_useast1_email_subscription" {
  count     = var.alarms_enabled ? 1 : 0
  provider  = aws.useast1
  topic_arn = local.use_cloudfront_arn ? local.sns_cloudfront_arn : aws_sns_topic.mpkhs_snstopic_useast1[0].arn

  #Email requires confirmation
  protocol = local.use_cloudfront_arn ? "sqs" : "email"
  endpoint = local.use_cloudfront_arn ? local.sqs_cloudfront_arn : aws_sns_topic.mpkhs_snstopic_useast1[0].arn
}

module "keydb" {
  source = "../../modules/keydb"

  environment = var.environment
  table_name  = local.keydb_table_name
  # Create only if enabled
  keydb_replica_regions = var.enable_dynamodb_replica ? [var.secondary_region] : []

  enable_dynamo_point_in_time_recovery = var.enable_dynamo_point_in_time_recovery
  enable_keydb_ttl                     = var.enable_keydb_ttl

  read_capacity                       = var.keydb_read_capacity
  autoscaling_read_max_capacity       = var.keydb_autoscaling_read_max_capacity
  autoscaling_read_target_percentage  = var.keydb_autoscaling_read_target_percentage
  write_capacity                      = var.keydb_write_capacity
  autoscaling_write_max_capacity      = var.keydb_autoscaling_write_max_capacity
  autoscaling_write_target_percentage = var.keydb_autoscaling_write_target_percentage
}

module "encryptionkeyservice" {
  source                     = "../../modules/encryptionkeyservice"
  encryption_key_service_jar = local.encryption_key_service_jar

  environment = var.environment

  dynamo_keydb = {
    name   = local.keydb_table_name
    arn    = module.keydb.key_db_arn
    region = var.primary_region
  }

  keymanagement_package_bucket = module.keymanagementpackagebucket_primary.bucket_id
  api_version                  = var.api_version

  logging_retention_days = var.cloudwatch_logging_retention_days
  #Alarms
  alarms_enabled                = var.alarms_enabled
  sns_topic_arn                 = var.alarms_enabled ? (var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn) : null
  alarm_eval_period_sec         = var.mpkhs_alarm_eval_period_sec
  lambda_error_threshold        = var.mpkhs_lambda_error_threshold
  lambda_error_log_threshold    = var.mpkhs_lambda_error_log_threshold
  lambda_max_duration_threshold = var.mpkhs_lambda_max_duration_threshold

  get_encryption_key_lambda_ps_client_shim_enabled = var.get_encryption_key_lambda_ps_client_shim_enabled

  api_gateway_id            = module.apigateway.api_gateway_id
  api_gateway_execution_arn = module.apigateway.api_gateway_execution_arn

  # VPC vars
  enable_vpc               = var.enable_vpc
  dynamodb_vpc_endpoint_id = var.enable_vpc ? module.vpc[0].dynamodb_vpc_endpoint_id : null
  lambda_sg_ids            = var.enable_vpc ? [module.vpc[0].allow_egress_sg_id, module.vpc[0].allow_internal_ingress_sg_id] : null
  private_subnet_ids       = var.enable_vpc ? module.vpc[0].private_subnet_ids : null
  custom_alarm_label       = var.custom_alarm_label
}

module "privatekeydomainprovider" {
  source = "../../modules/privatekeydomainprovider"
  count  = var.enable_domain_management ? 1 : 0

  environment = var.environment

  parent_domain_name = var.parent_domain_name

  domain_hosted_zone_id                = data.aws_route53_zone.hosted_zone[0].zone_id
  get_private_key_api_gateway_id       = module.apigateway.api_gateway_id
  get_private_key_api_gateway_stage_id = module.apigateway.gateway_stage_id
  service_subdomain                    = var.encryption_key_service_subdomain
}

module "publickeyhostingservice_primary" {
  source = "../../modules/publickeyhostingservice"

  #Global vars
  environment = var.environment

  #Lambda vars
  public_key_service_jar       = local.public_key_service_jar
  keymanagement_package_bucket = module.keymanagementpackagebucket_primary.bucket_id

  dynamo_keydb = {
    table_name = local.keydb_table_name
    arn        = module.keydb.key_db_arn
    region     = var.primary_region
  }
  get_public_key_cache_control_max_seconds              = var.get_public_key_cache_control_max_seconds
  get_public_key_lambda_memory_mb                       = var.get_public_key_lambda_memory_mb
  get_public_key_lambda_provisioned_concurrency_enabled = var.get_public_key_lambda_provisioned_concurrency_enabled
  get_public_key_lambda_provisioned_concurrency_count   = var.get_public_key_lambda_provisioned_concurrency_count
  #API Gateway vars
  api_version        = var.api_version
  api_env_stage_name = var.api_env_stage_name
  application_name   = var.application_name

  get_public_key_logging_retention_days = var.cloudwatch_logging_retention_days
  #Alarms
  public_key_service_alarms_enabled            = var.alarms_enabled
  get_public_key_sns_topic_arn                 = var.alarms_enabled ? (var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn) : null
  get_public_key_alarm_eval_period_sec         = var.mpkhs_alarm_eval_period_sec
  get_public_key_lambda_error_threshold        = var.mpkhs_lambda_error_threshold
  get_public_key_lambda_error_log_threshold    = var.mpkhs_lambda_error_log_threshold
  get_public_key_lambda_max_duration_threshold = var.mpkhs_lambda_max_duration_threshold
  get_public_key_api_max_latency_ms            = var.mpkhs_api_gw_max_latency_ms
  get_public_key_5xx_threshold                 = var.mpkhs_api_gw_5xx_threshold

  # VPC vars
  enable_vpc                          = var.enable_vpc
  coordinator_vpc_security_groups_ids = var.enable_vpc ? [module.vpc[0].allow_internal_ingress_sg_id, module.vpc[0].allow_egress_sg_id] : null
  coordinator_vpc_subnet_ids          = var.enable_vpc ? module.vpc[0].private_subnet_ids : null
  keydb_vpc_endpoint_id               = var.enable_vpc ? module.vpc[0].dynamodb_vpc_endpoint_id : null
  custom_alarm_label                  = var.custom_alarm_label
}

module "publickeyhostingservice_secondary" {
  source = "../../modules/publickeyhostingservice"

  providers = {
    aws = aws.secondary
  }

  #Global vars
  environment = var.environment

  #Lambda vars
  public_key_service_jar       = local.public_key_service_jar
  keymanagement_package_bucket = module.keymanagementpackagebucket_secondary.bucket_id

  dynamo_keydb = {
    table_name = local.keydb_table_name
    arn        = var.enable_dynamodb_replica ? replace(module.keydb.key_db_arn, var.primary_region, var.secondary_region) : module.keydb.key_db_arn
    region     = var.enable_dynamodb_replica ? var.secondary_region : var.primary_region
  }
  get_public_key_cache_control_max_seconds              = var.get_public_key_cache_control_max_seconds
  get_public_key_lambda_memory_mb                       = var.get_public_key_lambda_memory_mb
  get_public_key_lambda_provisioned_concurrency_enabled = var.get_public_key_lambda_provisioned_concurrency_enabled
  get_public_key_lambda_provisioned_concurrency_count   = var.get_public_key_lambda_provisioned_concurrency_count

  #API Gateway vars
  api_version        = var.api_version
  api_env_stage_name = var.api_env_stage_name
  application_name   = var.application_name

  get_public_key_logging_retention_days = var.cloudwatch_logging_retention_days
  #Alarms
  public_key_service_alarms_enabled            = var.alarms_enabled
  get_public_key_sns_topic_arn                 = var.alarms_enabled ? (var.secondary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs_snstopic_secondary[0].arn : var.secondary_region_sns_topic_arn) : null
  get_public_key_alarm_eval_period_sec         = var.mpkhs_alarm_eval_period_sec
  get_public_key_lambda_error_threshold        = var.mpkhs_lambda_error_threshold
  get_public_key_lambda_error_log_threshold    = var.mpkhs_lambda_error_log_threshold
  get_public_key_lambda_max_duration_threshold = var.mpkhs_lambda_max_duration_threshold
  get_public_key_api_max_latency_ms            = var.mpkhs_api_gw_max_latency_ms
  get_public_key_5xx_threshold                 = var.mpkhs_api_gw_5xx_threshold

  # VPC vars
  enable_vpc                          = var.enable_vpc
  coordinator_vpc_security_groups_ids = var.enable_vpc ? [module.vpc_secondary[0].allow_internal_ingress_sg_id, module.vpc_secondary[0].allow_egress_sg_id] : null
  coordinator_vpc_subnet_ids          = var.enable_vpc ? module.vpc_secondary[0].private_subnet_ids : null
  keydb_vpc_endpoint_id               = var.enable_vpc ? module.vpc_secondary[0].dynamodb_vpc_endpoint_id : null
  custom_alarm_label                  = var.custom_alarm_label
}

module "publickeyhostingcloudfront" {
  source = "../../modules/publickeyhostingcloudfront"

  environment               = var.environment
  primary_api_gateway_url   = module.publickeyhostingservice_primary.api_gateway_url
  secondary_api_gateway_url = module.publickeyhostingservice_secondary.api_gateway_url
  api_gateway_stage_name    = var.api_env_stage_name

  default_cloudfront_ttl_seconds = var.default_cloudfront_ttl_seconds
  max_cloudfront_ttl_seconds     = var.max_cloudfront_ttl_seconds
  min_cloudfront_ttl_seconds     = var.min_cloudfront_ttl_seconds

  #Domain
  enable_domain_management             = var.enable_domain_management
  parent_domain_name                   = var.parent_domain_name
  domain_hosted_zone_id                = var.enable_domain_management ? data.aws_route53_zone.hosted_zone[0].zone_id : null
  domain_name_to_domain_hosted_zone_id = var.public_key_service_domain_name_to_domain_hosted_zone_id
  service_subdomain                    = var.public_key_service_subdomain
  service_alternate_domain_names       = var.public_key_service_alternate_domain_names

  #Must be us-east-1
  get_public_key_sns_topic_arn = var.alarms_enabled ? (local.sns_cloudfront_arn == "" ? aws_sns_topic.mpkhs_snstopic_useast1[0].arn : local.sns_cloudfront_arn) : null

  #Alarms
  public_key_service_alarms_enabled                  = var.alarms_enabled
  get_public_key_alarm_eval_period_sec               = var.mpkhs_alarm_eval_period_sec
  get_public_key_cloudfront_5xx_threshold            = var.get_public_key_cloudfront_5xx_threshold
  get_public_key_cloudfront_cache_hit_threshold      = var.get_public_key_cloudfront_cache_hit_threshold
  get_public_key_cloudfront_origin_latency_threshold = var.get_public_key_cloudfront_origin_latency_threshold
  custom_alarm_label                                 = var.custom_alarm_label
}

module "apigateway" {
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
  api_gateway_sns_topic_arn         = var.alarms_enabled ? (var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn) : null
  custom_alarm_label                = var.custom_alarm_label
}

module "unifiedkeyhostingdashboard" {
  source = "../../modules/unifiedkeyhostingdashboard"
  count  = var.alarms_enabled ? 1 : 0

  account_id  = local.account_id
  environment = var.environment
  region      = var.primary_region

  get_encryption_key_lambda_version = module.encryptionkeyservice.lambda_version

  unified_keyhosting_api_gateway_id                = module.apigateway.api_gateway_id
  unified_keyhosting_dashboard_time_period_seconds = var.unified_keyhosting_dashboard_time_period_seconds
}

module "publickeyhostingdashboard" {
  source = "../../modules/publickeyhostingdashboard"
  count  = var.alarms_enabled ? 1 : 0

  account_id                                   = local.account_id
  environment                                  = var.environment
  primary_region                               = var.primary_region
  secondary_region                             = var.secondary_region
  get_public_key_dashboard_time_period_seconds = var.public_keyhosting_dashboard_time_period_seconds

  cloudfront_id                                        = module.publickeyhostingcloudfront.cloudfront_id
  get_public_key_primary_api_gateway_id                = module.publickeyhostingservice_primary.api_gateway_id
  get_public_key_primary_get_public_key_lambda_version = module.publickeyhostingservice_primary.lambda_version

  get_public_key_secondary_api_gateway_id                = module.publickeyhostingservice_secondary.api_gateway_id
  get_public_key_secondary_get_public_key_lambda_version = module.publickeyhostingservice_secondary.lambda_version
}
