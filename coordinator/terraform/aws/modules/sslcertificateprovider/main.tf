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

# Create certificate for any subdomain. Uses DNS validation
# This resource assume only one domain is being validated at a time.
resource "aws_acm_certificate" "ssl_certificate" {
  domain_name       = var.service_domain_name
  validation_method = "DNS"

  lifecycle {
    create_before_destroy = true
  }

  tags = { "environment" : var.environment }
}

# Create CNAME validation record
resource "aws_route53_record" "cert_validation" {
  allow_overwrite = true
  name            = tolist(aws_acm_certificate.ssl_certificate.domain_validation_options)[0].resource_record_name
  records         = [tolist(aws_acm_certificate.ssl_certificate.domain_validation_options)[0].resource_record_value]
  type            = tolist(aws_acm_certificate.ssl_certificate.domain_validation_options)[0].resource_record_type
  zone_id         = var.domain_hosted_zone_id
  ttl             = 60
}

# Resource that handles actual validation. Timeout at 30 minutes. If result is PENDING, then must be tried at a later time
resource "aws_acm_certificate_validation" "cert" {
  certificate_arn = aws_acm_certificate.ssl_certificate.arn
  validation_record_fqdns = [
    aws_route53_record.cert_validation.fqdn
  ]
  timeouts {
    create = "30m"
  }
}
