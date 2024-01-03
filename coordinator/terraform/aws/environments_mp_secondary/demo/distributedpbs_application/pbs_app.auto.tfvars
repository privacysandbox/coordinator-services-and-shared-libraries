# Must be updated
aws_region = "<deployment-region>"

# This must match the environment name in distributedpbs_base
# Referenced in comments below as <environment>
environment = "demo-b"

# This is the name of the peer/remote PBS coordinator
remote_environment = "demo-a"

# This is the account ID where the peer/remote coordinator is deployed.
remote_coordinator_aws_account_id = "<remote-account-id>"

## Uncomment one of the configuration blocks below depending on your type of deployment

## BEGIN - development default configuration values
# ec2_instance_type   = "m5.large"
# root_volume_size_gb = "512"
# beanstalk_solution_stack_name = "64bit Amazon Linux 2 v3.5.6 running Docker"

# budget_table_read_capacity  = 125
# budget_table_write_capacity = 125

# auth_table_read_initial_capacity   = 25
# auth_table_read_max_capacity       = 125
# auth_table_read_scale_utilization  = 70
# auth_table_write_initial_capacity  = 1
# auth_table_write_max_capacity      = 1
# auth_table_write_scale_utilization = 70
## END - development default configuration values

## BEGIN - production default configuration values
# ec2_instance_type   = "m5.8xlarge"
# root_volume_size_gb = "1024"

# budget_table_read_capacity  = 2500
# budget_table_write_capacity = 2500

# auth_table_read_initial_capacity   = 500
# auth_table_read_max_capacity       = 5000
# auth_table_read_scale_utilization  = 70
# auth_table_write_initial_capacity  = 5
# auth_table_write_max_capacity      = 20
# auth_table_write_scale_utilization = 70
## END - production default configuration values

# PBS Alarms
alarms_enabled                = false
alarm_notification_email      = "<notification email>"
pbs_cloudwatch_log_group_name = "/aws/elasticbeanstalk/demo-b-google-scp-pbs/docker/pbs-server/log.log"

# Alarm names are created with the following format:
# $Criticality$Alertname$CustomAlarmLabel
custom_alarm_label = "<CustomAlarmLabel>"

# Domain management:
# Environment name domain name will be:
# <service_subdomain>-<environment>.<parent_domain_name>
# With the values below:
# mp-pbs-demo-b.aws.coordinator.dev
# Note. An environment launched with domain management set to a value cannot be changed and update.
# This setting changes the load balancer type, which requires terminating the beanstalk environment (via console or cli)
# and doing another terraform apply with the updated setting.
enable_domain_management = true
service_subdomain        = "mp-pbs"
# Update with a pre-registed domain. It must exist.
parent_domain_name = "aws.coordinator.dev"

# Note. An environment deployed with a given VPC setting, cannot be changed and updated.
# This requires terminating the beanstalk environment (via console or cli) and recreating it with
# the new VPC setting via terraform apply
enable_vpc = true
# The number of availability zones to be used for VPC subnets
availability_zone_replicas = 3

application_environment_variables = {
  ##### Primary deployment information

  #### These values must be updated. If they are known at deployment time, then they
  #### can be provided, otherwise, they can be added in a subsequent terraform apply

  # Auth endpoint for primary environment
  google_scp_pbs_remote_auth_endpoint = "<primary-auth-url>"
  # Endpoint for primary environment
  google_scp_pbs_remote_host_address = "<primary-url>"
  # AWS Region of the primary environment
  google_scp_pbs_remote_cloud_region = "<primary-deployment-region>"
  # Enable site-based authorization in PBS
  google_scp_pbs_authorization_enable_site_based_authorization = "true"

  ## Uncomment one of the configuration blocks below depending on your type of deployment

  ## BEGIN - development default configuration values
  # # Set to the number of vCPUs of the EC2 instance type
  # google_scp_pbs_async_executor_threads_count    = "2"
  # google_scp_core_http2server_threads_count      = "4"
  # google_scp_pbs_io_async_executor_threads_count = "10"
  ## END - development default configuration values

  ## BEGIN - production default configuration values
  # # Set to the number of vCPUs of the EC2 instance type
  # google_scp_pbs_async_executor_threads_count = "32"
  # google_scp_core_http2server_threads_count      = "64"
  # google_scp_pbs_io_async_executor_threads_count = "160"
  ## END - production default configuration values
}
