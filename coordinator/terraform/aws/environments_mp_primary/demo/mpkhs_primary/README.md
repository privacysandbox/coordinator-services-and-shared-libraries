## Overview

Welcome to the README for setting up AWS Coordinator Key Hosting
Service!

Please follow the instructions below to deploy the Key Hosting
services in AWS.

## Pre-requisites

1. Install Terraform

## Instructions

### Deploy a New Infrastructure

1. Create a new directory in environments directory and copy the contents of
   environments/demo to the new environment directory
2. Configure AWS credentials
   using [AWS documentation](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html)
3. Edit the following required fields in the terraform configuration
   file `main.tf`:
    - terraform configurations
        - bucket: S3 bucket to store the terraform state
        - key: S3 object key to store the terraform state
        - region: Region to store terraform state
4. Edit the following required fields and any other fields
   in `mpkhs_primary.auto.tfvars`:
    - `environment`: name of the environment
    - `primary_region`: region to deploy services and keep terraform state
    - `secondary_region`: secondary region to deploy services
    - `enable_domain_management`: whether to manage custom domain for service
      APIs
    - `parent_domain_name`: domain name to be registered and used with APIs
    - `api_version`: version to be used for created APIs
    - `alarm_notification_email`: e-mail to send coordinator service alarm
      notifications
    - `enable_dynamodb_replica`: whether to set up dynamodb replica (needs to be
      false on ddb resource creation)
    - `enable_vpc`: whether to set up VPC for communication on internal AWS
      network (needs to be enabled in conjunction with enable_dynamodb_replica)
5. To deploy infrastructure in development mode, run the following commands to
   deploy the Key Hosting Services by setting `enable_vpc` and
   `enable_dynamodb_replica` to false:

```bash
$ terraform init
$ terraform plan
$ terraform apply -auto-approve
```

1. To deploy infrastructure in production mode, run the following commands to
   deploy the Key Hosting Services by setting `enable_vpc` and
   `enable_dynamodb_replica` to true:

```bash
$ terraform init
$ terraform plan -out=tfplan
$ terraform apply tfplan
$ terraform plan -out=tfplan  # Note the second plan and apply are workarounds to address a known terraform issue; See the troubleshooting section on `Provider produced inconsistent final plan`.
$ terraform apply tfplan
```

Note: Deploying in production mode is a two-step process. First, the
infrastructure must be deployed with `enable_vpc` and `enable_dynamodb_replica`
set to `false`. Then, it can be re-deployed with `enable_vpc`
and `enable_dynamodb_replica` set to `true`.

### Updating VPC configurations of an existing Infrastructure

---
**WARNING: Before proceeding, please read the following notice.**

Expect up to an hour of downtime when updating the VPC configurations of an
VPC-enabled infrastructure, as the operation will first need to destroy any
existing Lambdas, destroy the associated VPC resources, such as subnets and
security groups, and then recreate these resources. During this time, all
services, including `key_generation`, `get_public_key`, and `get_private_key`,
will be impacted. The impact on `get_public_key` is mitigated by CloudFront
providing public key caching to Chrome users. However, the impact on
the `get_private_key` service will be unavoidable, and the operators will be
unable to reach the service during the downtime****.
---

**Before Operation**

1. If necessary, adjust the CloudFront caching time of the `get_public_key`
   service to at least match the anticipated maintenance time window. For
   example, if the maintenance time window is scheduled to be an hour, then you
   can set the `min_cloudfront_ttl_seconds` and `default_cloudfront_ttl_seconds`
   variables to two hours, ensuring that all incoming requests can be served
   with cached response during the downtime.
2. Coordinate with the operators and inform them of the maintenance operation
   and time window.

**During Operation**

1. Delete existing Lambdas in the Service, including key
   generation, get private key, get public key in both primary and secondary
   regions.
2. Make necessary changes to VPC configurations, such as `VPC_CIDR`. Apply the
   changes via Terraform as shown below.

```
$ terraform plan -out=tfplan
$ terraform apply tfplan
```

## Troubleshooting

1. **Issue**: Encounter the following issue when running `terraform apply`:

```
│ Error: Resource precondition failed
│
│   on ../../../modules/vpc/main.tf line 47, in resource "aws_vpc" "coordinator_vpc":
│   47:       condition     = var.enable_dynamodb_replica
│     ├────────────────
│     │ var.enable_dynamodb_replica is false
│
│ DynamoDb replica must be enabled in the secondary region for VPC in the
│ secondary region to connect to the DynamoDb instance.  Cross-region access of
│ private resources is not supported without VPC peering."
```

**Solution**: This means that the VPC is enabled while DynamoDB replica is
disabled. This is an invalid configuration state. Set `enable_vpc`
and `enable_dynamodb_replica`
both to `true` before proceeding again.

2. **Issue**: When you attempt to apply infrastructure changes from Lambdas with
   VPC enabled to disabled, the VPC subnet and associated security groups will
   be stuck in deletion. For example, you see the following issue:

```text
│ Error: error deleting Lambda ENIs for EC2 Subnet (subnet-0a6bf17e70840cb70): error waiting for Lambda ENI (eni-090604bfff80bb773) to become available for detachment: timeout while waiting for state to become 'available' (last state: 'in-use', timeout: 45m0s)
```

**Solution**: This means that ENIs managed by the Lambda service are still
attached to the VPC subnet, preventing the subnet from being deleted. Workaround
suggested:

* (Recommended) Delete any existing Lambdas, including key generation, get
  private key, get public key in both primary and secondary regions, before
  running Terraform apply. You may use
  this [script](../../../../util_scripts/deploy/update_aws_spkhs_vpc) to perform this
  operation.
* (Dev Only) terraform destroy then terraform apply. Warning that all infra
  resources and states including DynamoDb will be wiped.

3. **Issue**: Whenever Terraform attempts to migrate an infrastructure from vpc
   disabled to enabled, then Terraform apply operation fails with an
   error `Provider produced inconsistent final plan`.

**Solution**: This is a known Terraform provider bug due to inconsistent state
of remote backend resources. See
support [article](https://support.hashicorp.com/hc/en-us/articles/1500006254562-Provider-Produced-Inconsistent-Results)
.

The workaround is simple: run `terraform apply` again.
