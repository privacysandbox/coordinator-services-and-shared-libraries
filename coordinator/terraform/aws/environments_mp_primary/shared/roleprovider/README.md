## Coordinator setup

Welcome to the README for setting up AWS Coordinator Role Provider!

Please follow the instructions below to set up the Coordinator Role Provider in AWS.

## Build pre-requisites
1. Install Terraform

## Steps to deploy services
1. Configure AWS credentials using [AWS documentation](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html)
2. Edit the following required fields in the terraform configuration file `main.tf`:
   - terraform configurations
      - bucket: S3 bucket to store the terraform state
      - key: S3 object key to store the terraform state
      - region: Region to store terraform state
3. Edit the following required fields and any other fields in `example.auto.tfvars`:
    - environment: name of the environment
    - aws_region: region to deploy services and keep terraform state
    - allowed_principals: AWS account IDs that will assume kms_decrypt_role
    - private_key_encryptor_arn: KMS key which will be used to decrypt
    - private_key_api_gateway_arn: API Gateway used to access the private key service
    - attestation_condition_keys: AWS Condition Keys for Nitro Enclaves

## Applying changes to set up Coordinator Role Provider

```
terraform init
terraform plan
terraform apply
```
