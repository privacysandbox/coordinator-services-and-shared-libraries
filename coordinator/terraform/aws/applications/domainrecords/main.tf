# Adds necessary records to route53 for domain before creating environments
# Only needs to be used once

terraform {
  required_version = "~> 1.2.3"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

provider "aws" {
  region = "us-east-1"
}

resource "aws_route53_zone" "hosted_zone" {
  name = var.domain_name
}

# Create CAA record for domain. Required for DNS validation
resource "aws_route53_record" "caa_record" {
  zone_id = aws_route53_zone.hosted_zone.zone_id
  name    = var.domain_name
  type    = "CAA"
  records = [
    "0 issue \"amazon.com\"",
    "0 issue \"pki.goog\"",
    "0 issue \"letsencrypt.org\""
  ]
  ttl = 60
}
