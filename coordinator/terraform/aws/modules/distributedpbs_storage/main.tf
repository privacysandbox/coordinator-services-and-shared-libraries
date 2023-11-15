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
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

resource "aws_s3_bucket" "pbs_journal_bucket" {
  bucket_prefix = "${var.environment_prefix}-scp-pbs-journals"
  force_destroy = var.journal_s3_bucket_force_destroy

  // Needed to work around a bug that would intermittently alter the server-side
  // encryption config between applies:
  // https://github.com/hashicorp/terraform-provider-aws/issues/23106#issuecomment-1099401600
  // https://github.com/hashicorp/terraform-provider-aws/issues/23106#issuecomment-1105537119
  // https://registry.terraform.io/providers/hashicorp/aws/3.75.1/docs/resources/s3_bucket_lifecycle_configuration#usage-notes
  lifecycle {
    ignore_changes = [server_side_encryption_configuration]
  }

  tags = {
    Name        = "${var.environment_prefix} PBS TX Journal Bucket"
    Environment = "${var.environment_prefix}"
  }
}

// We will not use a KMS key to encrypt this data
#tfsec:ignore:aws-s3-encryption-customer-key
resource "aws_s3_bucket_server_side_encryption_configuration" "journal_bucket_encryption" {
  bucket = aws_s3_bucket.pbs_journal_bucket.bucket

  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

resource "aws_s3_bucket_ownership_controls" "journal_bucket_ownership" {
  bucket = aws_s3_bucket.pbs_journal_bucket.id
  rule {
    object_ownership = "BucketOwnerPreferred"
  }
}

resource "aws_s3_bucket_acl" "journal_bucket_acl" {
  bucket = aws_s3_bucket.pbs_journal_bucket.id
  acl    = "private"

  depends_on = [
    aws_s3_bucket.pbs_journal_bucket,
    aws_s3_bucket_ownership_controls.journal_bucket_ownership
  ]
}

resource "aws_s3_bucket_versioning" "journal_bucket_versioning" {
  bucket = aws_s3_bucket.pbs_journal_bucket.id

  versioning_configuration {
    status = "Enabled"
  }

  depends_on = [
    aws_s3_bucket.pbs_journal_bucket
  ]
}

resource "aws_s3_bucket_public_access_block" "journal_bucket_block" {
  bucket                  = aws_s3_bucket.pbs_journal_bucket.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true

  depends_on = [
    aws_s3_bucket.pbs_journal_bucket
  ]
}

resource "aws_s3_bucket_lifecycle_configuration" "journal_bucket_object_lifecycle_config" {
  # Must have bucket versioning enabled first
  depends_on = [aws_s3_bucket_versioning.journal_bucket_versioning]

  bucket = aws_s3_bucket.pbs_journal_bucket.id

  rule {
    id = "Object deletion rule"

    expiration {
      days = 60
    }

    noncurrent_version_expiration {
      noncurrent_days = 10
    }

    status = "Enabled"
  }

  rule {
    id = "Delete marker rule"

    expiration {
      expired_object_delete_marker = true
    }

    status = "Enabled"
  }
}

resource "aws_s3_bucket_policy" "pbs_journal_bucket_deny_non_ssl_requests" {
  bucket = aws_s3_bucket.pbs_journal_bucket.id

  policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "DenyNonSslRequests",
      "Action": "s3:*",
      "Effect": "Deny",
      "Resource": [
        "${aws_s3_bucket.pbs_journal_bucket.arn}",
        "${aws_s3_bucket.pbs_journal_bucket.arn}/*"
      ],
      "Condition": {
        "Bool": {
          "aws:SecureTransport": "false"
        }
      },
      "Principal": "*"
    }
  ]
}
EOF
}

# Note: The below tfsec issues are ignored because DynamoDB uses encryption
# by default.
#tfsec:ignore:aws-dynamodb-enable-at-rest-encryption
#tfsec:ignore:aws-dynamodb-table-customer-key
resource "aws_dynamodb_table" "budget_keys_table" {
  name = "${var.environment_prefix}-google-scp-pbs-budget-keys"

  billing_mode   = "PROVISIONED"
  read_capacity  = var.budget_table_read_capacity
  write_capacity = var.budget_table_write_capacity

  hash_key  = "Budget_Key"
  range_key = "Timeframe"

  attribute {
    name = "Budget_Key"
    type = "S"
  }

  attribute {
    name = "Timeframe"
    type = "S"
  }

  point_in_time_recovery {
    enabled = var.budget_table_enable_point_in_time_recovery
  }

  tags = {
    Name        = "${var.environment_prefix}-google-scp-pbs-budget-keys"
    Environment = "${var.environment_prefix}"
  }
}

# Note: The below tfsec issues are ignored because DynamoDB uses encryption
# by default and this table only holds transient data.
#tfsec:ignore:aws-dynamodb-enable-at-rest-encryption
#tfsec:ignore:aws-dynamodb-table-customer-key
#tfsec:ignore:aws-dynamodb-enable-recovery
resource "aws_dynamodb_table" "partition_lock_table" {
  name = "${var.environment_prefix}-google-scp-pbs-partition-lock"

  billing_mode   = "PROVISIONED"
  read_capacity  = var.partition_lock_table_read_capacity
  write_capacity = var.partition_lock_table_write_capacity

  point_in_time_recovery {
    enabled = var.partition_lock_table_enable_point_in_time_recovery
  }

  hash_key = "LockId"

  attribute {
    name = "LockId"
    type = "S"
  }

  tags = {
    Name        = "${var.environment_prefix}-google-scp-pbs-partition-lock"
    Environment = "${var.environment_prefix}"
  }
}

resource "aws_dynamodb_table_item" "partition_lock_table_default_global_partition_entry" {
  table_name = aws_dynamodb_table.partition_lock_table.name
  hash_key   = aws_dynamodb_table.partition_lock_table.hash_key

  lifecycle {
    ignore_changes = all
  }

  item = <<ITEM
  {
    "LockId": {"S": "0"},
    "LeaseOwnerId": {"S": "0"},
    "LeaseExpirationTimestamp": {"S": "0"},
    "LeaseOwnerServiceEndpointAddress": {"S": "0"}
  }
  ITEM
}
