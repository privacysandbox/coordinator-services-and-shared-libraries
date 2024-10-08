locals {
  pbs_tls_alternate_names_length = var.pbs_tls_alternate_names != null ? length(var.pbs_tls_alternate_names) : 0
}
output "pbs_alternate_instance_domain_record_data" {
  description = "Alternate DNS record data."
  value = [
    for i in range(local.pbs_tls_alternate_names_length) :
    {
      "alternate_domain" = google_certificate_manager_dns_authorization.pbs_alternate_instance[i].dns_resource_record
    }
  ]
}
