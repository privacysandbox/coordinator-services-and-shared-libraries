output "pbs_authorization_table_v2_auth_records" {
  description = "Map of assumed role ARNs to string set of adtech sites. Key = IAM role ARN. Value = String set of adtech sites"
  value = tomap(
    {
      for name, content in aws_dynamodb_table_item.pbs_authorization_table_v2_item :
      jsondecode(content.item)["adtech_identity"]["S"] =>
      jsondecode(content.item)["adtech_sites"]["SS"]
  })
}
