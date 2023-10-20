module "serverless-connector" {
  count      = var.create_connectors ? 1 : 0
  source     = "terraform-google-modules/network/google//modules/vpc-serverless-connector-beta"
  project_id = var.project_id
  vpc_connectors = [
    for index, region in tolist(var.regions) : {
      name    = "${var.environment}-${region}"
      region  = region
      network = module.vpc_network.network_name
      # 16 IPs since max number of connectors is limited to 16 by the VPC
      # service. Here we are using the 10.1.0.0/24 block, enough for up to 16
      # regions. Only 2 are expected.
      ip_cidr_range  = "10.1.0.${index * 16}/28"
      subnet_name    = null
      machine_type   = var.connector_machine_type
      min_instances  = 2
      max_instances  = 10
      max_throughput = 1000
    }
  ]
}
