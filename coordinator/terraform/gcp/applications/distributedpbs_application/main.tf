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

provider "google" {
  region  = var.region
  project = var.project
}

locals {
  pbs_auth_allowed_principals           = var.pbs_remote_coordinator_service_account_email != "" ? concat(var.pbs_auth_allowed_principals, ["serviceAccount:${var.pbs_service_account_email}", "serviceAccount:${var.pbs_remote_coordinator_service_account_email}"]) : concat(var.pbs_auth_allowed_principals, ["serviceAccount:${var.pbs_service_account_email}"])
  pbs_artifact_registry_repository_name = var.pbs_artifact_registry_repository_name != "" ? var.pbs_artifact_registry_repository_name : "${var.environment}-scp-pbs-artifact-registry-repo"
  pbs_instance_target_tag               = "${var.environment}-pbs-network-tag"
  service_subdomain_suffix              = var.service_subdomain_suffix != null ? var.service_subdomain_suffix : "-${var.environment}"
  pbs_domain                            = "${var.service_subdomain}${local.service_subdomain_suffix}.${var.parent_domain_name}"
  pbs_service_endpoint                  = var.enable_domain_management ? "https://${local.pbs_domain}" : "https://${local.pbs_ip_address}"
  pbs_ip_address                        = google_compute_global_address.pbs_ip_address.address
  pbs_cidr                              = "10.125.0.0/20"
  pbs_main_port_name                    = "${var.environment}-pbs-main-port"
  pbs_health_port_name                  = "${var.environment}-pbs-health-port"
  pbs_health_port                       = 9000
}

# Reserve IP address for PBS.
resource "google_compute_global_address" "pbs_ip_address" {
  project = var.project
  name    = "${var.environment}-pbs-ip-address"
}

module "pbs_a_record" {
  count  = var.enable_domain_management ? 1 : 0
  source = "../../modules/distributedpbs_domain_a_record"

  environment = var.environment

  parent_domain_project = var.parent_domain_project
  parent_domain_name    = var.parent_domain_name
  pbs_ip_address        = local.pbs_ip_address
  pbs_domain            = local.pbs_domain
}

module "pbs_auth_storage" {
  source = "../../modules/distributedpbs_auth_db"

  environment                                   = var.environment
  region                                        = var.region
  pbs_auth_spanner_instance_processing_units    = var.pbs_auth_spanner_instance_processing_units
  pbs_auth_spanner_database_retention_period    = var.pbs_auth_spanner_database_retention_period
  pbs_auth_spanner_database_deletion_protection = var.pbs_auth_spanner_database_deletion_protection
}

module "pbs_auth" {
  source = "../../modules/distributedpbs_auth_service"

  project_id  = var.project
  environment = var.environment
  region      = var.region

  auth_cloud_function_handler_path              = var.auth_cloud_function_handler_path
  pbs_auth_cloudfunction_min_instances          = var.pbs_auth_cloudfunction_min_instances
  pbs_auth_cloudfunction_max_instances          = var.pbs_auth_cloudfunction_max_instances
  pbs_auth_cloudfunction_timeout_seconds        = var.pbs_auth_cloudfunction_timeout_seconds
  pbs_auth_cloudfunction_instance_concurrency   = var.pbs_auth_cloudfunction_instance_concurrency
  pbs_auth_cloudfunction_instance_available_cpu = var.pbs_auth_cloudfunction_instance_available_cpu
  pbs_auth_cloudfunction_memory_mb              = var.pbs_auth_cloudfunction_memory_mb
  pbs_auth_package_bucket_force_destroy         = var.pbs_auth_package_bucket_force_destroy
  pbs_auth_spanner_instance_name                = module.pbs_auth_storage.pbs_auth_spanner_instance_name
  pbs_auth_spanner_database_name                = module.pbs_auth_storage.pbs_auth_spanner_database_name
  pbs_auth_v2_spanner_table_name                = module.pbs_auth_storage.pbs_auth_spanner_authorization_v2_table_name
  pbs_auth_allowed_principals                   = local.pbs_auth_allowed_principals
  limit_traffic_to_internal_and_lb              = var.enable_domain_management

  depends_on = [
    module.pbs_auth_storage
  ]
}

module "pbs_storage" {
  source = "../../modules/distributedpbs_storage"

  environment                                    = var.environment
  region                                         = var.region
  pbs_cloud_storage_journal_bucket_force_destroy = var.pbs_cloud_storage_journal_bucket_force_destroy
  pbs_cloud_storage_journal_bucket_versioning    = var.pbs_cloud_storage_journal_bucket_versioning
  pbs_spanner_instance_processing_units          = var.pbs_spanner_instance_processing_units
  pbs_spanner_database_retention_period          = var.pbs_spanner_database_retention_period
  pbs_spanner_database_deletion_protection       = var.pbs_spanner_database_deletion_protection
}

module "pbs_managed_instance_group_environment" {
  source = "../../modules/distributedpbs_managed_instance_group_environment"

  project     = var.project
  environment = var.environment
  region      = var.region

  pbs_artifact_registry_repository_name = local.pbs_artifact_registry_repository_name
  pbs_image_tag                         = var.pbs_image_tag

  pbs_cloud_storage_journal_bucket_name = module.pbs_storage.pbs_cloud_storage_journal_bucket_name
  pbs_spanner_instance_name             = module.pbs_storage.pbs_spanner_instance_name
  pbs_spanner_database_name             = module.pbs_storage.pbs_spanner_database_name
  pbs_spanner_budget_key_table_name     = module.pbs_storage.pbs_spanner_budget_key_table_name
  pbs_spanner_partition_lock_table_name = module.pbs_storage.pbs_spanner_partition_lock_table_name

  machine_type           = var.machine_type
  root_volume_size_gb    = var.root_volume_size_gb
  pbs_autoscaling_policy = var.pbs_autoscaling_policy

  pbs_cloud_logging_enabled    = var.pbs_cloud_logging_enabled
  pbs_cloud_monitoring_enabled = var.pbs_cloud_monitoring_enabled

  pbs_application_environment_variables = var.pbs_application_environment_variables
  pbs_auth_audience_url                 = module.pbs_auth.pbs_auth_audience_url
  pbs_service_account_email             = var.pbs_service_account_email
  enable_domain_management              = var.enable_domain_management
  pbs_domain                            = "https://${local.pbs_domain}"

  vpc_network_id           = google_compute_network.vpc_default_network.self_link
  vpc_subnet_id            = google_compute_subnetwork.mig_subnetwork.self_link
  network_target_tag       = local.pbs_instance_target_tag
  pbs_custom_vm_tags       = var.pbs_custom_vm_tags
  named_ports              = [{ name = "${local.pbs_main_port_name}", port = var.pbs_main_port }, { name = "${local.pbs_health_port_name}", port = local.pbs_health_port }]
  main_port                = var.pbs_main_port
  health_check_port        = local.pbs_health_port
  enable_health_check      = var.enable_health_check
  enable_public_ip_address = var.enable_public_ip_address
}

module "pbs_lb" {
  source = "../../modules/distributedpbs_load_balancer"

  environment = var.environment
  region      = var.region
  project_id  = var.project

  enable_domain_management       = var.enable_domain_management
  pbs_domain                     = local.pbs_domain
  parent_domain_name             = var.parent_domain_name
  parent_domain_project          = var.parent_domain_project
  pbs_ip_address                 = local.pbs_ip_address
  pbs_auth_cloudfunction_name    = module.pbs_auth.pbs_auth_cloudfunction_name
  pbs_managed_instance_group_url = module.pbs_managed_instance_group_environment.pbs_instance_group_url
  pbs_named_port                 = local.pbs_main_port_name
  pbs_main_port                  = var.pbs_main_port
  pbs_health_check_port          = local.pbs_health_port
  pbs_vpc_network_id             = google_compute_network.vpc_default_network.self_link
  pbs_instance_target_tag        = local.pbs_instance_target_tag
  pbs_instance_allow_ssh         = var.pbs_instance_allow_ssh
  pbs_tls_alternate_names        = var.pbs_tls_alternate_names

  depends_on = [
    module.pbs_a_record,
  ]
}

resource "google_compute_network" "vpc_default_network" {
  # Max 63 characters
  name                    = "${var.environment}-pbs-mig-vpc"
  description             = "VPC created for PBS environment ${var.environment}"
  auto_create_subnetworks = false
  mtu                     = 1500
}

resource "google_compute_subnetwork" "mig_subnetwork" {
  name                     = "${var.environment}-pbs-mig-hc-vpc-subnet"
  ip_cidr_range            = local.pbs_cidr
  network                  = google_compute_network.vpc_default_network.self_link
  region                   = var.region
  private_ip_google_access = true
}

# Cloud router with NAT to provide internet to VMs without external IPs.
resource "google_compute_router" "pbs" {
  name    = "${var.environment}-pbs-router"
  project = var.project
  region  = var.region
  network = google_compute_network.vpc_default_network.id

  bgp {
    asn = 64514
  }
}
moved {
  from = module.vpc_nat.google_compute_router.router[0]
  to   = google_compute_router.pbs
}

resource "random_string" "nat" {
  length  = 6
  upper   = false
  special = false
}
moved {
  from = module.vpc_nat.random_string.name_suffix
  to   = random_string.nat
}

resource "google_compute_router_nat" "pbs" {
  name    = "cloud-nat-${random_string.nat.result}"
  project = var.project
  region  = var.region
  router  = google_compute_router.pbs.name

  source_subnetwork_ip_ranges_to_nat = "ALL_SUBNETWORKS_ALL_IP_RANGES"
  nat_ip_allocate_option             = "AUTO_ONLY"
}
moved {
  from = module.vpc_nat.google_compute_router_nat.main
  to   = google_compute_router_nat.pbs
}

resource "google_compute_firewall" "ingress_instance_firewall" {
  project     = var.project
  name        = "${var.environment}-pbs-instance-fw"
  description = "Firewall rule to allow communications for PBS instances from the evnironment ${var.environment}."
  network     = google_compute_network.vpc_default_network.self_link

  priority = 100

  source_ranges = [
    local.pbs_cidr
  ]

  target_tags = [local.pbs_instance_target_tag]

  allow {
    protocol = "tcp"
    ports    = ["${var.pbs_main_port}"]
  }

  allow {
    protocol = "icmp"
  }

  log_config {
    metadata = "INCLUDE_ALL_METADATA"
  }
}
