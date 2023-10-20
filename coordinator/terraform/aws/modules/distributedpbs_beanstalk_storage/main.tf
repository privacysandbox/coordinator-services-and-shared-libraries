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

// We will not enable versioning for this bucket
#tfsec:ignore:aws-s3-enable-versioning
resource "aws_s3_bucket" "beanstalk_bucket" {
  bucket_prefix = "${var.environment_prefix}-scp-pbs-beanstalk"
  force_destroy = var.beanstalk_app_bucket_force_destroy

  // Needed to work around a bug that would intermittently alter the server-side
  // encryption config between applies:
  // https://github.com/hashicorp/terraform-provider-aws/issues/23106#issuecomment-1099401600
  // https://github.com/hashicorp/terraform-provider-aws/issues/23106#issuecomment-1105537119
  // https://registry.terraform.io/providers/hashicorp/aws/3.75.1/docs/resources/s3_bucket_lifecycle_configuration#usage-notes
  lifecycle {
    ignore_changes = [server_side_encryption_configuration]
  }

  tags = {
    Name        = "${var.environment_prefix} Beanstalk version objects"
    Environment = "${var.environment_prefix}"
  }
}

// We will not use a KMS key to encrypt this data
#tfsec:ignore:aws-s3-encryption-customer-key
resource "aws_s3_bucket_server_side_encryption_configuration" "bucket_encryption" {
  bucket = aws_s3_bucket.beanstalk_bucket.bucket

  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

resource "aws_s3_bucket_ownership_controls" "beanstalk_bucket_ownership" {
  bucket = aws_s3_bucket.beanstalk_bucket.id
  rule {
    object_ownership = "BucketOwnerPreferred"
  }
}

resource "aws_s3_bucket_acl" "beanstalk_bucket_bucket_acl" {
  bucket = aws_s3_bucket.beanstalk_bucket.id
  acl    = "private"

  depends_on = [
    aws_s3_bucket.beanstalk_bucket,
    aws_s3_bucket_ownership_controls.beanstalk_bucket_ownership
  ]
}

resource "aws_s3_bucket_public_access_block" "beanstalk_bucket_block" {
  bucket                  = aws_s3_bucket.beanstalk_bucket.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

resource "aws_s3_bucket_policy" "beanstalk_bucket_deny_non_ssl_requests" {
  bucket = aws_s3_bucket.beanstalk_bucket.id

  policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "DenyNonSslRequests",
      "Action": "s3:*",
      "Effect": "Deny",
      "Resource": [
        "${aws_s3_bucket.beanstalk_bucket.arn}",
        "${aws_s3_bucket.beanstalk_bucket.arn}/*"
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

resource "aws_s3_bucket" "pbs_elb_access_logs" {
  bucket_prefix = "pbs-${var.environment_prefix}-elb-access-logs"
  acl           = "private"

  force_destroy = var.beanstalk_app_bucket_force_destroy

  versioning {
    enabled = true
  }

  lifecycle_rule {
    enabled = true

    noncurrent_version_expiration {
      days = 60
    }
  }

  tags = {
    Name        = "elb_access_logs"
    Application = "elb"
  }
}

resource "aws_s3_bucket_public_access_block" "pbs_elb_access_logs" {
  bucket = aws_s3_bucket.pbs_elb_access_logs.id

  block_public_acls       = true
  block_public_policy     = true
  restrict_public_buckets = true
  ignore_public_acls      = true
}

resource "aws_s3_bucket_policy" "pbs_elb_access_logs_policy" {
  bucket = aws_s3_bucket.pbs_elb_access_logs.id
  policy = jsonencode({
    Version = "2012-10-17"
    Id      = "elbAccessLogsPolicy"
    Statement = [
      {
        Effect = "Allow"
        Principal = {
          AWS = "arn:aws:iam::${var.pbs_aws_lb_arn["${var.aws_region}"]}:root"
        }
        Action   = ["s3:PutObject"]
        Resource = ["arn:aws:s3:::${aws_s3_bucket.pbs_elb_access_logs.id}/AWSLogs/${var.aws_account_id}/*"]
      }
    ]
  })
}
