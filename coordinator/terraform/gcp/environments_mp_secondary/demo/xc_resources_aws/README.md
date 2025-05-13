# Overview

Please follow the instructions below to set up resources required for
AWS cross cloud operation of MPKHS.

## Pre-requisites

1. Install Terraform

## Instructions

### Deploy a New Infrastructure

1. Create a new directory in environments directory and copy the contents of
   environments/demo to the new environment directory
2. Configure GCP credentials using gcloud with service account or default
   account
3. Configure AWS credentials
   using [AWS documentation](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html)
4. Edit the following required fields in the terraform configuration
   file `terraform.tf`:
    - terraform configurations
        - bucket: gcs bucket to store the terraform state
        - prefix: perfix of the path to store the terraform state
5. Edit the following required variables and any other optional variables
   (listed in `xc_resources_aws_variables.tf`) in `main.tf`:
    - `environment`: Environment name.
    - `project_id`: GCP project ID.
    - `key_generation_service_account_unique_id`: Unique ID of service account
      used by key generation instance. Output of mpkhs_primary.
    - `aws_account_id_to_role_names`: A map of operator account IDs to role
      within the account that will be allowed to use the coordinator. If roles
      list is empty access is granted to entire account.
    - `aws_attestation_enabled`: If attestation will be enabled. Can be disabled
      only for development purposes.
    - `aws_attestation_pcr_allowlist`: List of PCR0s to allowlist for Nitro
      Enclave attestation. Ignored if attestation is disabled.

```bash
terraform init
terraform plan
terraform apply -auto-approve
```
