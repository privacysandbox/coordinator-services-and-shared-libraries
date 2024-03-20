output "encryption_key_service_cloudfunction_url" {
  value = module.encryptionkeyservice.encryption_key_service_cloudfunction_url
}

output "encryption_key_service_loadbalancer_ip" {
  value = module.encryptionkeyservice.encryption_key_service_loadbalancer_ip
}

output "encryption_key_base_url" {
  value = var.enable_domain_management ? "https://${local.encryption_key_domain}" : "http://${module.encryptionkeyservice.encryption_key_service_loadbalancer_ip}"
}

output "key_storage_cloudfunction_url" {
  value = module.keystorageservice.key_storage_cloudfunction_url
}

output "key_storage_service_loadbalancer_ip" {
  value = module.keystorageservice.load_balancer_ip
}

output "key_storage_base_url" {
  value = var.enable_domain_management ? "https://${local.key_storage_domain}" : "http://${module.keystorageservice.load_balancer_ip}"
}

output "key_encryption_key_id" {
  value = google_kms_crypto_key.key_encryption_key.id
}

output "workload_identity_pool_provider_name" {
  value = module.workload_identity_pool.workload_identity_pool_provider_name
}

output "wip_allowed_service_account" {
  value = module.workload_identity_pool.wip_allowed_service_account
}

output "wip_verified_service_account" {
  value = module.workload_identity_pool.wip_verified_service_account
}
