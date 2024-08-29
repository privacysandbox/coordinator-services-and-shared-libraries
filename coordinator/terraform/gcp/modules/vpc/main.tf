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
  egress_internet_tag = "egress-internet"
}

resource "google_compute_network" "network" {
  name         = "${var.environment}-network"
  project      = var.project_id
  routing_mode = "GLOBAL"
  # Unlike AWS, concept of private/public subnets do not translate well to GCP
  # because routes to internet gateway are managed at the VPC network level.
  # Such that the route either exists for all subnets or not. GCP provides more
  # direct control of internet access like restricting route to internet gateway
  # to specific instance tags.
  #
  # Subnets will be created within the 10.128.0.0/9 range. Each subnet is /20
  # with 4096 addresses.
  #
  # Don't need all subnets, but auto-create simplifies TF config.
  auto_create_subnetworks = true
  # Routes for each subnet are automatically created. Delete the default internet
  # gateway route and replace it with one restricted to the
  # `egress_internet_tag`.
  delete_default_routes_on_create = true
}
moved {
  from = module.vpc_network.module.vpc.google_compute_network.network
  to   = google_compute_network.network
}

# for_each is used as this resource moved from a module that used for_each.
# Despite there being a single instance, this enables the moved block to work.
resource "google_compute_route" "egress_internet" {
  for_each         = toset(["${var.environment}-egress-internet"])
  name             = each.key
  project          = var.project_id
  network          = google_compute_network.network.name
  description      = "Route to the Internet."
  dest_range       = "0.0.0.0/0"
  tags             = [local.egress_internet_tag]
  next_hop_gateway = "default-internet-gateway"
}
moved {
  from = module.vpc_network.module.routes.google_compute_route.route
  to   = google_compute_route.egress_internet
}

# Cloud NAT to provide internet to VMs without external IPs.
# For tooling compatibility, we avoid using external modules. As this module
# uses "for_each", we vendor the module as moving all resources is impractical.
module "vpc_nat" {
  source   = "../cloud-nat"
  for_each = var.regions

  project_id    = var.project_id
  network       = google_compute_network.network.id
  region        = each.value
  create_router = true
  router        = "${var.environment}-router"
}
