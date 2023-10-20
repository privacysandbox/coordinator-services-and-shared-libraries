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

locals {
  origin_group_id     = "${var.environment}_get_public_keys_api_gateway_group"
  primary_origin_id   = "${var.environment}_get_public_keys_primary_origin"
  secondary_origin_id = "${var.environment}_get_public_keys_secondary_origin"
  public_key_domain   = "${var.service_subdomain}.${var.parent_domain_name}"
}

# Domain record
# Cannot be refactored to its own module due to viewer_certificate section in aws_cloudfront_distribution
module "sslcertificate_public_key_service" {
  source = "../sslcertificateprovider"
  count  = var.enable_domain_management ? 1 : 0

  environment                          = var.environment
  service_domain_name                  = local.public_key_domain
  subject_alternative_names            = var.service_alternate_domain_names
  domain_hosted_zone_id                = var.domain_hosted_zone_id
  domain_name_to_domain_hosted_zone_id = var.domain_name_to_domain_hosted_zone_id
}

resource "aws_route53_record" "cloudfront_alias" {
  count   = var.enable_domain_management ? 1 : 0
  zone_id = var.domain_hosted_zone_id
  name    = local.public_key_domain
  type    = "A"

  alias {
    name                   = aws_cloudfront_distribution.get_public_keys_cloudfront.domain_name
    zone_id                = aws_cloudfront_distribution.get_public_keys_cloudfront.hosted_zone_id
    evaluate_target_health = false
  }
}

# Define cloudfront distribution
# The public key service is publicly accessible and generic so there is no clear WAF criteria.
#tfsec:ignore:aws-cloudfront-enable-waf
resource "aws_cloudfront_distribution" "get_public_keys_cloudfront" {

  origin_group {
    origin_id = local.origin_group_id

    #All error codes cloudfront allows for failover
    #https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/high_availability_origin_failover.html
    failover_criteria {
      status_codes = [403, 404, 500, 502, 503, 504]
    }

    member {
      origin_id = local.primary_origin_id
    }

    member {
      origin_id = local.secondary_origin_id
    }
  }

  origin {
    # Cloudfront only accepts domain
    domain_name = replace(var.primary_api_gateway_url, "/^https?://([^/]*).*/", "$1")
    origin_path = "/${var.api_gateway_stage_name}"
    origin_id   = local.primary_origin_id

    #Necessary config settings
    custom_origin_config {
      http_port              = 80
      https_port             = 443
      origin_protocol_policy = "https-only"
      origin_ssl_protocols = [
      "TLSv1.2"]
    }
  }

  origin {
    # Cloudfront only accepts domain
    domain_name = replace(var.secondary_api_gateway_url, "/^https?://([^/]*).*/", "$1")
    origin_path = "/${var.api_gateway_stage_name}"
    origin_id   = local.secondary_origin_id

    #Necessary config settings
    custom_origin_config {
      http_port              = 80
      https_port             = 443
      origin_protocol_policy = "https-only"
      origin_ssl_protocols = [
      "TLSv1.2"]
    }
  }

  aliases = var.enable_domain_management ? concat([local.public_key_domain], var.service_alternate_domain_names) : null
  enabled = true
  comment = var.environment

  #Define public key cache properties
  default_cache_behavior {
    allowed_methods = [
      "GET",
    "HEAD"]
    cached_methods = [
      "GET",
    "HEAD"]
    target_origin_id = local.origin_group_id

    #TTLs in seconds
    min_ttl     = var.min_cloudfront_ttl_seconds
    max_ttl     = var.max_cloudfront_ttl_seconds
    default_ttl = var.default_cloudfront_ttl_seconds

    forwarded_values {
      cookies {
        forward = "none"
      }
      query_string = false
    }
    #Only allow https requests
    viewer_protocol_policy = "https-only"
  }

  #Deploy to all edge locations
  price_class = "PriceClass_All"

  #Custom domain. Cannot be its own module
  viewer_certificate {
    cloudfront_default_certificate = var.enable_domain_management ? null : true
    acm_certificate_arn            = var.enable_domain_management ? module.sslcertificate_public_key_service[0].acm_certificate_validation_arn : null
    ssl_support_method             = var.enable_domain_management ? "sni-only" : null
    minimum_protocol_version       = "TLSv1.2_2021"
  }

  #No geographic restrictions
  restrictions {
    geo_restriction {
      restriction_type = "none"
    }
  }

  logging_config {
    include_cookies = false
    bucket          = aws_s3_bucket.get_public_keys_cloudfront_logs_bucket.bucket_domain_name
    prefix          = "cloudfront_logs"
  }

  depends_on = [
    // CloudFront requires the S3 bucket for logs enables ACL access.
    aws_s3_bucket_acl.get_public_keys_cloudfront_logs_bucket_acl
  ]
}

# Canonical User ID is different than Account ID
data "aws_canonical_user_id" "canonical_user_id" {}

# S3 bucket for cloudfront logs, created in us-east-1
# Logs do not need versioning and does not have durability requirement.
#tfsec:ignore:aws-s3-enable-versioning
resource "aws_s3_bucket" "get_public_keys_cloudfront_logs_bucket" {
  bucket_prefix = "${var.environment}-pubkey-cf-logs-"
}

resource "aws_s3_bucket_public_access_block" "get_public_keys_cloudfront_logs_bucket_public_access_block" {
  bucket                  = aws_s3_bucket.get_public_keys_cloudfront_logs_bucket.id
  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

resource "aws_s3_bucket_ownership_controls" "get_public_keys_cloudfront_logs_bucket_ownership" {
  bucket = aws_s3_bucket.get_public_keys_cloudfront_logs_bucket.id
  rule {
    object_ownership = "BucketOwnerPreferred"
  }
}

resource "aws_s3_bucket_policy" "get_public_keys_cloudfront_logs_bucket_deny_non_ssl_requests" {
  bucket = aws_s3_bucket.get_public_keys_cloudfront_logs_bucket.id

  policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "DenyNonSslRequests",
      "Action": "s3:*",
      "Effect": "Deny",
      "Resource": [
        "${aws_s3_bucket.get_public_keys_cloudfront_logs_bucket.arn}",
        "${aws_s3_bucket.get_public_keys_cloudfront_logs_bucket.arn}/*"
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

# S3 bucket acl for cloudfront logs, created in us-east-1
resource "aws_s3_bucket_acl" "get_public_keys_cloudfront_logs_bucket_acl" {
  bucket = aws_s3_bucket.get_public_keys_cloudfront_logs_bucket.id

  access_control_policy {
    # Define bucket permissions so terraform does not overwrite
    grant {
      grantee {
        id   = data.aws_canonical_user_id.canonical_user_id.id
        type = "CanonicalUser"
      }
      permission = "FULL_CONTROL"
    }

    # Must give permission to AWS Log Delivery Group to ensure logs are written to bucket
    # https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/AccessLogs.html#AccessLogsBucketAndFileOwnership
    grant {
      grantee {
        id   = "c4c1ede66af53448b93c283ce9448c4ba468c9432aa01d700d3878632f77d2d0"
        type = "CanonicalUser"
      }
      permission = "FULL_CONTROL"
    }
    owner {
      id = data.aws_canonical_user_id.canonical_user_id.id
    }
  }

  depends_on = [
    aws_s3_bucket.get_public_keys_cloudfront_logs_bucket,
    aws_s3_bucket_ownership_controls.get_public_keys_cloudfront_logs_bucket_ownership
  ]
}
