## Coordinator A setup

Welcome to the README for setting up AWS Multi Party Coordinator Key Hosting
Service in Primary Coordinator configuration!

Please follow the instructions below to set up the Coordinator Key Hosting services in AWS.

## Build pre-requisites
1. Install Terraform

## Steps to deploy services
1. Create a new directory in environments directory and copy the contents of environments/demo to the new environment directory
2. Configure AWS credentials using [AWS documentation](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html)
3. Edit the following required fields in the terraform configuration file `main.tf`:
   - terraform configurations
      - bucket: S3 bucket to store the terraform state
      - key: S3 object key to store the terraform state
      - region: Region to store terraform state
4. Edit the following required fields and any other fields in `example.auto.tfvars`:
   - environment: name of the environment
   - primary_region: region to deploy services and keep terraform state
   - secondary_region: secondary region to deploy services
   - enable_domain_management: whether to manage custom domain for service APIs
   - api_version: version to be used for created APIs
   - alarm_notification_email: e-mail to send coordinator service alarm notifications
   - enable_dynamodb_replica: whether to set up dynamodb replica (needs to be false on ddb resource creation)

## Applying changes to set up Coordinator B Key Hosting Services

```
terraform init
terraform plan
terraform apply
```
