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
      source = "hashicorp/aws"
      # 3.75.0 is needed for aws_s3_bucket_acl.
      version = "~> 3.0, > 3.75.0"
    }
  }
}

# Used for lambda deployment package, no durability requirement.
#tfsec:ignore:aws-s3-enable-versioning
resource "aws_s3_bucket" "coordinator_shared_bucket" {
  bucket_prefix = "${var.environment}-${var.name_suffix}-"
}

resource "aws_s3_bucket_public_access_block" "coordinator_shared_bucket_public_access_block" {
  bucket                  = aws_s3_bucket.coordinator_shared_bucket.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

resource "aws_s3_bucket_ownership_controls" "coordinator_shared_bucket_ownership" {
  bucket = aws_s3_bucket.coordinator_shared_bucket.id
  rule {
    object_ownership = "BucketOwnerPreferred"
  }
}

resource "aws_s3_bucket_acl" "coordinator_shared_bucket_acl" {
  bucket = aws_s3_bucket.coordinator_shared_bucket.id
  acl    = "private"

  depends_on = [
    aws_s3_bucket.coordinator_shared_bucket,
    aws_s3_bucket_ownership_controls.coordinator_shared_bucket_ownership
  ]
}

resource "aws_s3_bucket_policy" "coordinator_shared_bucket_deny_non_ssl_requests" {
  bucket = aws_s3_bucket.coordinator_shared_bucket.id

  policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "DenyNonSslRequests",
      "Action": "s3:*",
      "Effect": "Deny",
      "Resource": [
        "${aws_s3_bucket.coordinator_shared_bucket.arn}",
        "${aws_s3_bucket.coordinator_shared_bucket.arn}/*"
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
