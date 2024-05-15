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
    google = {
      source  = "hashicorp/google"
      version = ">= 4.36"
    }
  }
}

locals {
  # Build map of projects and services to enable IAM audit logging
  audit_log_projects = toset(var.enable_audit_log ? [var.project_id, var.wip_allowed_service_account_project_id_override] : [])
  audit_log_projects_services = flatten([
    for project in local.audit_log_projects : [
      {
        "project" : project,
        "service" : "iam.googleapis.com",
      },
      {
        "project" : project,
        "service" : "sts.googleapis.com",
      }
    ]
  ])
  # Terraform can’t for_each over a list - convert to map with unique keys
  audit_log_projects_services_map = { for item in local.audit_log_projects_services : "${item.project},${item.service}" => item }
}

resource "google_iam_workload_identity_pool" "workload_identity_pool" {
  project = var.project_id

  # Limited to 32 characters
  workload_identity_pool_id = var.workload_identity_pool_id
  # Use ID because display name also has character limitation
  display_name = var.workload_identity_pool_id
  description  = var.workload_identity_pool_description
}

resource "google_iam_workload_identity_pool_provider" "workload_identity_pool" {
  project = var.project_id

  workload_identity_pool_id = google_iam_workload_identity_pool.workload_identity_pool.workload_identity_pool_id

  # Limited to 32 characters
  workload_identity_pool_provider_id = var.workload_identity_pool_provider_id
  # Use ID because display name also has character limitation
  display_name = var.workload_identity_pool_provider_id
  description  = var.wip_provider_description

  # Mapping grants access to subset of identities
  # https://cloud.google.com/iam/docs/workload-identity-federation?_ga=2.215242071.-153658173.1651444935#mapping
  # Confidential Space token claims https://cloud.google.com/confidential-computing/confidential-space/docs/reference/token-claims
  attribute_mapping = {
    "google.subject" : "\"psb::\" + assertion.submods.container.image_digest + \"::\" + assertion.submods.gce.instance_id + \"::\" + assertion.submods.gce.project_number",
    "attribute.image_digest" : "assertion.submods.container.image_digest",
    "attribute.image_reference" : "assertion.submods.container.image_reference"

  }

  # Attestation condition expression to allow access only when conditions are met.
  # https://cloud.google.com/iam/docs/workload-identity-federation?_ga=2.39606883.-153658173.1651444935#conditions
  attribute_condition = (var.enable_attestation ?
    <<-EOT
  assertion.swname == '${var.assertion_tee_swname}'
  && ${jsonencode(var.assertion_tee_support_attributes)}.all(a, a in assertion.submods.confidential_space.support_attributes)
  && '${google_service_account.wip_allowed.email}' in assertion.google_service_accounts
  EOT
  : "")

  oidc {
    # Only allow STS requests from confidential computing
    allowed_audiences = ["https://sts.googleapis.com"]
    issuer_uri        = "https://confidentialcomputing.googleapis.com/"
  }
}

# Service Account that defines permissions for Workload Identities
resource "google_service_account" "wip_verified" {
  project = var.project_id
  # Service account id has a 30 character limit
  account_id   = var.wip_verified_service_account_id
  display_name = var.wip_verified_service_account_display_name
}

# Allowlist IAM binding for all if enable_attestation = false
resource "google_service_account_iam_member" "workload_identity_member_disabled_attestation" {
  count              = var.enable_attestation ? 0 : 1
  service_account_id = google_service_account.wip_verified.id
  member             = "principalSet://iam.googleapis.com/${google_iam_workload_identity_pool.workload_identity_pool.name}/*"
  role               = "roles/iam.workloadIdentityUser"
}

# Allowlist IAM binding for container image digests
resource "google_service_account_iam_member" "workload_identity_member_image_digest" {
  for_each           = toset(var.assertion_tee_container_image_hash_list)
  service_account_id = google_service_account.wip_verified.id
  member             = "principalSet://iam.googleapis.com/${google_iam_workload_identity_pool.workload_identity_pool.name}/attribute.image_digest/${each.value}"
  role               = "roles/iam.workloadIdentityUser"
}

# Allowlist IAM binding for container image reference
resource "google_service_account_iam_member" "workload_identity_member_image_reference" {
  for_each           = toset(var.assertion_tee_container_image_reference_list)
  service_account_id = google_service_account.wip_verified.id
  member             = "principalSet://iam.googleapis.com/${google_iam_workload_identity_pool.workload_identity_pool.name}/attribute.image_reference/${each.value}"
  role               = "roles/iam.workloadIdentityUser"
}
# Service Account which is allowed to make calls to WIP
resource "google_service_account" "wip_allowed" {
  project = var.wip_allowed_service_account_project_id_override
  # Service account id has a 30 character limit
  account_id   = var.wip_allowed_service_account_id
  display_name = var.wip_allowed_service_account_display_name
}

# Allow principals to create tokens for Service Account that is allowed to make calls to WIP
resource "google_service_account_iam_member" "allowed_service_account_token_creator" {
  for_each           = setunion(var.allowed_wip_iam_principals, var.allowed_wip_user_group != null ? ["group:${var.allowed_wip_user_group}"] : [])
  service_account_id = google_service_account.wip_allowed.id
  member             = each.key
  role               = "roles/iam.serviceAccountTokenCreator"
}

# Enable IAM and STS Audit Logs, to allow diagnosing issues with Adtechs use of the WIP
resource "google_project_iam_audit_config" "audit_log" {
  for_each = local.audit_log_projects_services_map

  project = each.value.project
  service = each.value.service

  audit_log_config {
    log_type = "DATA_READ"
  }
  audit_log_config {
    log_type = "DATA_WRITE"
  }
  audit_log_config {
    log_type = "ADMIN_READ"
  }
}
