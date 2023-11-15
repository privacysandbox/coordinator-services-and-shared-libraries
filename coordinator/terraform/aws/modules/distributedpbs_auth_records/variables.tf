variable "privacy_budget_auth_table_name" {
  description = "DynamoDB table name of distributed pbs auth table"
  type        = string
}

variable "allowed_principals_map_v2" {
  type        = map(list(string))
  description = "Map of AWS account IDs that will the assume coordinator_assume_role associated with their list of sites."
}

variable "coordinator_assume_role_arns" {
  type        = map(string)
  description = "Map of Account ID to corresponding assumed role ARNs. Key = Account ID. Value = assumed role ARN."
}
