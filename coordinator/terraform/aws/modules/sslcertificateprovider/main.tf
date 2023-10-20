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
  domain_name_to_domain_hosted_zone_id = length(var.domain_name_to_domain_hosted_zone_id) > 0 ? var.domain_name_to_domain_hosted_zone_id : { "${var.service_domain_name}" = var.domain_hosted_zone_id }
}

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
  domain_name               = var.service_domain_name
  subject_alternative_names = var.subject_alternative_names
  validation_method         = "DNS"

  lifecycle {
    create_before_destroy = true
  }

  tags = { "environment" : var.environment }
}

# Create CNAME validation record
resource "aws_route53_record" "cert_validation" {
  for_each = {
    for dvo in aws_acm_certificate.ssl_certificate.domain_validation_options : dvo.domain_name => {
      name    = dvo.resource_record_name
      record  = dvo.resource_record_value
      type    = dvo.resource_record_type
      zone_id = local.domain_name_to_domain_hosted_zone_id[dvo.domain_name]
    }
  }
  allow_overwrite = true
  name            = each.value.name
  records         = [each.value.record]
  ttl             = 60
  type            = each.value.type
  zone_id         = each.value.zone_id
}


# Resource that handles actual validation. Timeout at 30 minutes. If result is PENDING, then must be tried at a later time
resource "aws_acm_certificate_validation" "cert" {
  certificate_arn = aws_acm_certificate.ssl_certificate.arn
  validation_record_fqdns = [
    for record in aws_route53_record.cert_validation : record.fqdn
  ]
  timeouts {
    create = "30m"
  }
}
