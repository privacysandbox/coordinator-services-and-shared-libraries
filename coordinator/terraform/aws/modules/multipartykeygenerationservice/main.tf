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

terraform {
  required_version = "~> 1.2.3"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

locals {
  required_enclave_ssm_parameters = {
    KEY_GENERATION_QUEUE_URL           = var.keyjobqueue_url,
    KEY_STORAGE_SERVICE_BASE_URL       = var.keystorage_api_base_url,
    KMS_KEY_URI                        = "aws-kms://${module.worker_enclave_encryption_key.key_arn}",
    KEY_DB_NAME                        = var.keydb_table_name,
    KEY_DB_REGION                      = var.keydb_region,
    COORDINATOR_B_ASSUME_ROLE_ARN      = var.coordinator_b_assume_role_arn,
    ENCRYPTION_KEY_SIGNATURE_ALGORITHM = local.signature_key_algorithm,
  }
  enclave_ssm_parameters = merge(local.required_enclave_ssm_parameters,
    var.enable_public_key_signature ? {
      ENCRYPTION_KEY_SIGNATURE_KEY_ID = aws_kms_key.encryption_key_signature_key[0].arn
    } : {},
    var.key_id_type != "" ? {
      KEY_ID_TYPE = var.key_id_type
    } : {},
  )
}

data "aws_ami" "split_key_image" {
  most_recent = true
  owners      = var.ami_owners
  filter {
    name = "name"
    values = [
      // AMI name format {ami-name}--YYYY-MM-DD'T'hh-mm-ssZ
      "${var.ami_name}--*"
    ]
  }
  filter {
    name = "state"
    values = [
      "available"
    ]
  }
}

resource "aws_iam_instance_profile" "split_key_profile" {
  name = "${var.environment}_${var.ec2_iam_role_name}Profile"
  role = aws_iam_role.enclave_role.name
}

resource "aws_launch_template" "split_key_template" {
  name = "${var.environment}_${var.split_key_rotation_template_name}"

  credit_specification {
    cpu_credits = "standard"
  }

  iam_instance_profile {
    name = aws_iam_instance_profile.split_key_profile.name
  }

  image_id = data.aws_ami.split_key_image.id

  instance_type = var.instance_type

  monitoring {
    enabled = true
  }

  enclave_options {
    enabled = true
  }

  tag_specifications {
    resource_type = "instance"
    tags = {
      Name        = "${var.service}-${var.environment}"
      service     = var.service
      environment = var.environment
    }
  }

  metadata_options {
    http_endpoint = "enabled"
    http_tokens   = "required"
  }

  user_data = filebase64("${path.module}/ec2_startup.sh")

  vpc_security_group_ids = var.enable_vpc ? var.secure_vpc_sg_ids : aws_security_group.split_key_sg[*].id
}

################################################################################
# Parameter Store
################################################################################

resource "aws_ssm_parameter" "enclave_parameter_store" {
  for_each = local.enclave_ssm_parameters
  name     = format("scp-%s-%s", var.environment, each.key)
  type     = "String"
  value    = each.value
  tags = {
    name        = format("scp-%s-%s", var.environment, each.key)
    service     = "scp"
    environment = var.environment
    role        = each.key
  }
}

################################################################################
# IAM Role
################################################################################

resource "aws_iam_role" "enclave_role" {
  name = "${var.environment}_${var.ec2_iam_role_name}"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Sid    = ""
        Principal = {
          Service = "ec2.amazonaws.com"
        }
      },
    ]
  })

  managed_policy_arns = [
    "arn:aws:iam::aws:policy/AmazonSSMManagedInstanceCore",
  ]
}

data "aws_iam_policy_document" "split_key_gen_policy" {
  statement {
    sid       = "AllowAssumeRole"
    effect    = "Allow"
    actions   = ["sts:AssumeRole"]
    resources = [var.coordinator_b_assume_role_arn]
  }

  statement {
    sid       = "AllowKMSEncryption"
    effect    = "Allow"
    actions   = ["kms:Encrypt"]
    resources = [module.worker_enclave_encryption_key.key_arn]
  }

  dynamic "statement" {
    # The contents of the list below are arbitrary, but must be of length one.
    for_each = var.enable_public_key_signature ? [1] : []

    content {
      sid       = "AllowKMSSign"
      effect    = "Allow"
      actions   = ["kms:Sign"]
      resources = [aws_kms_key.encryption_key_signature_key[0].arn]
    }
  }

  statement {
    sid       = "AllowSQSAccess"
    effect    = "Allow"
    actions   = ["sqs:ReceiveMessage", "sqs:DeleteMessage"]
    resources = [var.keyjobqueue_arn]
  }

  statement {
    sid       = "AllowSSMGetParameters"
    effect    = "Allow"
    actions   = ["ssm:GetParameters"]
    resources = [for v in aws_ssm_parameter.enclave_parameter_store : v.arn]
  }

  statement {
    sid       = "AllowDDBAccess"
    effect    = "Allow"
    actions   = ["dynamodb:GetItem", "dynamodb:PutItem", "dynamodb:Scan"]
    resources = [var.keydb_arn]
  }

  statement {
    sid       = "AllowEC2DescribeTags"
    effect    = "Allow"
    actions   = ["ec2:DescribeTags"]
    resources = ["*"]
  }

  dynamic "statement" {
    # The contents of the list below are arbitrary, but must be of length one.
    for_each = var.enable_vpc ? [1] : []

    content {
      sid       = "DenyDdbAccessFromAnyOtherEndpoints"
      effect    = "Deny"
      actions   = ["dynamodb:*"]
      resources = [var.keydb_arn]
      condition {
        test     = "StringNotEquals"
        values   = [var.dynamodb_vpc_endpoint_id]
        variable = "aws:sourceVpce"
      }
    }
  }

  dynamic "statement" {
    # The contents of the list below are arbitrary, but must be of length one.
    for_each = var.enable_vpc ? [1] : []

    content {
      sid       = "AllowDdbAccessFromSpecificEndpoint"
      effect    = "Allow"
      actions   = ["dynamodb:*"]
      resources = [var.keydb_arn]
      condition {
        test     = "StringEquals"
        values   = [var.dynamodb_vpc_endpoint_id]
        variable = "aws:sourceVpce"
      }
    }
  }
}

resource "aws_iam_role_policy" "split_key_gen_policy" {
  name = "split_key_gen_enclave_role_policy"
  role = aws_iam_role.enclave_role.id

  policy = data.aws_iam_policy_document.split_key_gen_policy.json
}

################################################################################
# Autoscaling
################################################################################

resource "aws_autoscaling_group" "split_key_rotation_group" {
  name                = "${var.environment}_${var.asg_name}"
  vpc_zone_identifier = var.enable_vpc ? var.secure_vpc_subnet_ids : [for s in aws_subnet.split_key_subnet : s.id]
  desired_capacity    = var.initial_capacity_ec2_instances
  max_size            = var.max_ec2_instances
  min_size            = var.min_ec2_instances

  launch_template {
    id = aws_launch_template.split_key_template.id
    # "$latest" doesn't trigger an instance refresh automatically.
    version = aws_launch_template.split_key_template.latest_version
  }

  lifecycle {
    create_before_destroy = false
    # The autoscaling group must be destroyed in order to properly clean up the
    # default VPC attached to the auto-scaling group.  Using the key gen policy
    # as a proxy to indicate this change.
    replace_triggered_by = [
      aws_iam_role_policy.split_key_gen_policy.policy
    ]
  }

  instance_refresh {
    strategy = "Rolling"
    preferences {
      # Allow all instances to be replaced instantly, doesn't need to be HA.
      instance_warmup        = 0
      min_healthy_percentage = 0
    }
  }

  enabled_metrics = ["GroupInServiceInstances"]

  depends_on = [aws_route_table_association.split_key_route_table_assoc]
}
