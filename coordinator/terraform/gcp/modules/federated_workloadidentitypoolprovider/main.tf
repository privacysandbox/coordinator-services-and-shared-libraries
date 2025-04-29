# Copyright 2024 Google LLC
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
      version = ">= 6.29.0"
    }
  }
}

locals {
  aws_account_ids = toset(keys(var.aws_account_id_to_role_names))
  account_id_to_params = {
    for account_id in local.aws_account_ids : account_id => {
      # Limited to 32 characters
      # Warning: Operators rely on the format of this id, it is set automatically on the operator side.
      wip_id          = "${var.environment}-aws-wip-${account_id}"
      wip_description = "Workload Identity Pool to manage GCP access for operators using federation for AWS Account ${account_id}"
      # Limited to 32 characters
      # Warning: Operators rely on the format of this id, it is set automatically on the operator side.
      wipp_id          = "wipp-${account_id}"
      wipp_description = "WIP Provider to manage federation for AWS Account ${account_id}"
      # Limited to 30 characters
      # Warning: Operators rely on the format of this id, it is set automatically on the operator side.
      wip_federated_service_account_id           = "${var.environment}-fed-aws-${account_id}"
      wip_federated_service_account_display_name = "${var.environment} Federated Operator User - AWS Account ${account_id}"
    }
  }

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
  # Terraform cannot for_each over a list - convert to map with unique keys
  audit_log_projects_services_map = { for item in local.audit_log_projects_services : "${item.project},${item.service}" => item }
}

resource "google_iam_workload_identity_pool" "federated_workload_identity_pool" {
  for_each = local.aws_account_ids

  project                   = var.project_id
  workload_identity_pool_id = local.account_id_to_params[each.key].wip_id
  # Use ID because display name also has character limitation
  display_name = local.account_id_to_params[each.key].wip_id
  description  = local.account_id_to_params[each.key].wip_description
}

resource "google_iam_workload_identity_pool_provider" "federated_workload_identity_pool" {
  for_each = local.aws_account_ids

  project                   = var.project_id
  workload_identity_pool_id = google_iam_workload_identity_pool.federated_workload_identity_pool[each.key].workload_identity_pool_id
  # Limited to 32 characters
  workload_identity_pool_provider_id = local.account_id_to_params[each.key].wipp_id
  # Use ID because display name also has character limitation
  display_name = local.account_id_to_params[each.key].wipp_id
  description  = local.account_id_to_params[each.key].wipp_description

  # Define mapping from AWS to GCP token.
  # https://cloud.google.com/iam/docs/workload-identity-federation?_ga=2.215242071.-153658173.1651444935#mapping
  attribute_mapping = {
    "google.subject" : "assertion.arn",
    "attribute.aws_account_id" : "assertion.account",
    "attribute.aws_role_name" : "assertion.arn.extract('assumed-role/{role}/')"
  }

  # Authenticate only GCP tokens for which attribute condition evaluates to true.
  # If no roles are provided check only account_id.
  # https://cloud.google.com/iam/docs/workload-identity-federation?_ga=2.215242071.-153658173.1651444935#condition
  attribute_condition = (length(var.aws_account_id_to_role_names[each.key]) == 0 ?
    "attribute.aws_account_id == '${each.key}'" :
    "attribute.aws_account_id == '${each.key}' && attribute.aws_role_name in ['${join("', '", var.aws_account_id_to_role_names[each.key])}']"
  )

  aws {
    account_id = each.key
  }
}

# # Service Account that defines permissions for Workload Identities
resource "google_service_account" "wip_federated" {
  for_each = local.aws_account_ids

  project = var.wip_allowed_service_account_project_id_override
  # Service account id has a 30 character limit
  account_id   = local.account_id_to_params[each.key].wip_federated_service_account_id
  display_name = local.account_id_to_params[each.key].wip_federated_service_account_display_name
}

resource "google_service_account_iam_member" "workload_identity_member_account" {
  for_each = local.aws_account_ids

  service_account_id = google_service_account.wip_federated[each.key].id
  # Grant membership to all identities authenticated within the pool.
  member = "principalSet://iam.googleapis.com/${google_iam_workload_identity_pool.federated_workload_identity_pool[each.key].name}/attribute.aws_account_id/${each.key}"
  role   = "roles/iam.workloadIdentityUser"
}

# Allow WIP service account to create Open ID Token to access Cloud Run
resource "google_service_account_iam_member" "wip_sa_id_token_creator" {
  for_each = local.aws_account_ids

  service_account_id = google_service_account.wip_federated[each.key].id
  member             = "serviceAccount:${google_service_account.wip_federated[each.key].email}"
  role               = "roles/iam.serviceAccountOpenIdTokenCreator"
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
