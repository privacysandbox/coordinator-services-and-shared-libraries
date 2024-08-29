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
    google = ">= 4.36.0"
  }
}

data "google_organization" "org" {
  count  = var.customer_id == "" ? 1 : 0
  domain = var.organization_domain
}

locals {
  customer_id = var.customer_id != "" ? var.customer_id : data.google_organization.org[0].directory_customer_id
}

resource "google_cloud_identity_group" "group" {
  display_name         = var.group_name
  description          = "${var.group_name} Google Group via Terraform"
  parent               = "customers/${local.customer_id}"
  initial_group_config = var.initial_group_config

  group_key {
    id = "${var.group_name}@${var.organization_domain}"
  }

  labels = {
    "cloudidentity.googleapis.com/groups.discussion_forum" = ""
  }
}
moved {
  from = module.group.google_cloud_identity_group.group
  to   = google_cloud_identity_group.group
}

resource "google_cloud_identity_group_membership" "owners" {
  for_each = toset(var.owners)

  group = google_cloud_identity_group.group.id

  preferred_member_key {
    id = each.key
  }

  # MEMBER role must be specified. The order of roles should not be changed.
  roles {
    name = "OWNER"
  }
  roles {
    name = "MEMBER"
  }
}
moved {
  from = module.group.google_cloud_identity_group_membership.owners
  to   = google_cloud_identity_group_membership.owners
}

resource "google_cloud_identity_group_membership" "managers" {
  for_each = toset(var.managers)

  group = google_cloud_identity_group.group.id

  preferred_member_key {
    id = each.key
  }

  # MEMBER role must be specified. The order of roles should not be changed.
  roles {
    name = "MEMBER"
  }
  roles {
    name = "MANAGER"
  }
}
moved {
  from = module.group.google_cloud_identity_group_membership.managers
  to   = google_cloud_identity_group_membership.managers
}

resource "google_cloud_identity_group_membership" "members" {
  for_each = toset(var.members)

  group = google_cloud_identity_group.group.id

  preferred_member_key {
    id = each.key
  }

  roles {
    name = "MEMBER"
  }
}
moved {
  from = module.group.google_cloud_identity_group_membership.members
  to   = google_cloud_identity_group_membership.members
}
