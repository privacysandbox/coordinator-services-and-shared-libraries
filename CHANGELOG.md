# Changelog

## [1.11.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.10.0...v1.11.0) (2024-10-07)

- **Important note**
  - **[GCP only]** Path to the auth lambda handler should now be a packaged artifact and `auth_cloud_function_requirements_path` should be removed, example path:
    ```
    auth_cloud_function_handler_path = "../../../dist/auth_cloud_function_handler.zip"
    ```

### Changes

  - Added OTel java libraries and Config Modules
  - Added new BlobStorageClient API to get blob bytes by range
  - Changed http status code for an unknown exception to `500` instead of `403`
  - Cleaned up c++ code base for PBS
  - Corrected type of `allowed_wip_iam_principals` var in demo.auto.tfvars
  - Created http connection pool metrics for PBS
  - Enabled OTel instrumentation for BudgetKeyTimeframeManager and JournalService
  - Removed use of site enrollment and multi-origin feature flags
  - Updated PBS client to send trusted service client version in the outgoing request header
  - Updated container dependencies
  - [AWS only] Enabled SQS msg queue based autoscaling of Keygeneration EC2 Nitro instances
  - [AWS only] Tuned Origin Latency Alarm to support minimum QPS and lower latency threshold to align with Android
  - [AWS only] Updated DNS management terraform for supporting Cross-Cloud PBS
  - [AWS only] Upgraded AWS lambda function to Python version 3.9
  - [GCP only] Added new optimized PBS client for simplified transaction protocol
  - [GCP only] Changed Load Balancing Schema from `EXTERNAL` to `EXTERNAL_MANAGED`
  - [GCP only] Added option for bucket versioning in AdTech setup
  - [GCP only] Cleanups in MPKHS Terraform
  - [GCP only] Packaged PBS Auth cloud function into zip at build time
  - [GCP only] Removed `kms:Decrypt` permission from key generation service instance
  - [GCP only] Updated Cloud Function to use Java 17
  - [GCP only] Updated Key Generation Service to be regional instead of zonal
  - [GCP only] Updated the minimum GCP Terraform provider version for MPKHS to 5.21
  - [GPC only] Set default input variables for GCP Terraform

## [1.10.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.9.0...v1.10.0) (2024-08-16)

### Manual Deployment Steps Required
 - [AWS only] Steps to migrate from Launch Configuration to Launch Templates for PBS
   1. In the AWS console, navigate to EC2 > Settings > Data Protection & Security > IMDS Defaults and set the following values:
    - Set Instance Metadata service to "Enabled"
    - Set Metadata version to "V2 only (token required)"
    - Set Access to tags in metadata to "Enabled"
    - Set Metadata response hop limits to "2"
   2. Run `terraform apply`
   3. On success, in `<name>.auto.tfvars`, set a new variable `enable_imds_v2` to `true`
   4. Run `terraform apply`
   5. In the AWS console, navigate to EC2 > Auto Scaling Groups. Confirm that the Elastic Beanstalk auto scaling group is using a Launch Template. It should be visible under the Launch Template / Configuration column.
 - [GCP only] The provider definition has been removed from the allowed operator group module. If using Application Default Credentials, the provider may need to be manually configured at the root module level. For details, see -
      https://registry.terraform.io/providers/hashicorp/google/latest/docs/resources/cloud_identity_group.

### Changes

  - Fixed OTel exporter timeout and interval
  - Refactored OTel metric names and labels
  - [AWS only] Adjusted request throttling for AWS Auth Function from 10000 to 2300
  - [AWS only] Fixed AWS "PBS Transactions" Dashboard
  - [AWS only] Migrated PBS EC2 instances from using Launch Configurations to Launch Templates
  - [GCP only] Added LOG_EXECUTION_ID to cloudfunction environment to avoid terraform plan changes on repeated terraform applies
  - [GCP only] Enabled Public Access Prevention for Cloud Storage
  - [GCP only] Forced provider update during PBS deployment
  - [GCP only] Migrated PBS loadbalancer cert from SSL to CertManager
  - [GCP only] Updated GCP secret manager module to new provider API
  - [GCP only] Updated mpkhs coordinator README on resolving edge case error

### Rollback steps

  - [GCP only] Terraform module usage was refactored. In case of rollback, the following commands must be run manually:
    - *mpkhs_primary*
      ```
      terraform state mv module.multipartykeyhosting_primary.module.vpc.google_compute_route.egress_internet module.multipartykeyhosting_primary.module.vpc.module.vpc_network.module.routes.google_compute_route.route
      terraform state mv module.multipartykeyhosting_primary.module.vpc.google_compute_network.network module.multipartykeyhosting_primary.module.vpc.module.vpc_network.module.vpc.google_compute_network.network
      ```
    - *mpkhs_secondary*
      ```
      terraform state mv module.multipartykeyhosting_secondary.module.vpc.google_compute_route.egress_internet module.multipartykeyhosting_secondary.module.vpc.module.vpc_network.module.routes.google_compute_route.route
      terraform state mv module.multipartykeyhosting_secondary.module.vpc.google_compute_network.network module.multipartykeyhosting_secondary.module.vpc.module.vpc_network.module.vpc.google_compute_network.network
      terraform state mv module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_backend_service.key_storage 'module.multipartykeyhosting_secondary.module.keystorageservice.module.lb-http_serverless_negs.google_compute_backend_service.default["default"]'
      terraform state mv module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_global_address.key_storage 'module.multipartykeyhosting_secondary.module.keystorageservice.module.lb-http_serverless_negs.google_compute_global_address.default[0]'
      terraform state mv 'module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_global_forwarding_rule.https[0]' 'module.multipartykeyhosting_secondary.module.keystorageservice.module.lb-http_serverless_negs.google_compute_global_forwarding_rule.https[0]'
      terraform state mv 'module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_managed_ssl_certificate.key_storage[0]' 'module.multipartykeyhosting_secondary.module.keystorageservice.module.lb-http_serverless_negs.google_compute_managed_ssl_certificate.default[0]'
      terraform state mv 'module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_target_https_proxy.key_storage[0]' 'module.multipartykeyhosting_secondary.module.keystorageservice.module.lb-http_serverless_negs.google_compute_target_https_proxy.default[0]'
      terraform state mv module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_url_map.key_storage 'module.multipartykeyhosting_secondary.module.keystorageservice.module.lb-http_serverless_negs.google_compute_url_map.default[0]'
      ```
    - *distributedpbs_application*
      ```
      terraform state mv module.distributedpbs_application.google_compute_router.pbs module.distributedpbs_application.module.vpc_nat.google_compute_router.router[0]
      terraform state mv module.distributedpbs_application.random_string.nat module.distributedpbs_application.module.vpc_nat.random_string.name_suffix
      terraform state mv module.distributedpbs_application.google_compute_router_nat.pbs module.distributedpbs_application.module.vpc_nat.google_compute_router_nat.main
      ```
    - *allowedoperatorgroup*
      ```
      terraform state mv module.allowedoperatorgroup.module.allowed_user_group.google_cloud_identity_group.group module.allowedoperatorgroup.module.allowed_user_group.module.group.google_cloud_identity_group.group
      terraform state mv module.allowedoperatorgroup.module.allowed_user_group.google_cloud_identity_group_membership.owners module.allowedoperatorgroup.module.allowed_user_group.module.group.google_cloud_identity_group_membership.owners
      terraform state mv module.allowedoperatorgroup.module.allowed_user_group.google_cloud_identity_group_membership.managers module.allowedoperatorgroup.module.allowed_user_group.module.group.google_cloud_identity_membership.managers
      terraform state mv module.allowedoperatorgroup.module.allowed_user_group.google_cloud_identity_group_membership.members module.allowedoperatorgroup.module.allowed_user_group.module.group.google_cloud_identity_group_membership.members
      ```
  - [GCP only] Migrating PBS loadbalancer cert from Classic Load Balancer SSL cert to CertManager does not require any config changes, however to rollback
     1. Detach the certificate map from the PBS target proxy `gcloud compute target-https-proxies update <environment>-pbs-proxy --clear-certificate-map`
     2. Deploy the previous stable release

  - [AWS only] A patch to the PBS Elastic Beanstalk Environment terraform from the v1.9.0 source code will be required for successful rollback:
     1. Remove `coordinator/terraform/aws/modules/distributedpbs_beanstalk_environment/files/autoscaling-launch.config`
     2. Delete the data block `data "local_file" "autoscaling_launch_config"` (Line 104-106)
     3. Delete the source block `data.local_file.autoscaling_launch_config.content` (Line 139-142)
     4. Rebuild the `multiparty_coordinator_tar` and deploy.

## [1.9.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.8.1...v1.9.0) (2024-06-27)

### Changes
  - Added `reporting_site` optional parameter to CreateJob API schema.
  - Copied tink patch to fix newer absl.
  - Removed trailing slash and port from reporting origin in the PBS request handling to avoid PRIVACY_BUDGET_AUTHORIZATION_ERROR.
  - Updated log message level for budget exhausted from error to warning.
  - Updated Budget Recovery tool to process new json format.
  - Updated dependencies: Bazel, Protobuf, gRPC, Abseil, GoogleTest, google-cloud-cpp.
  - [GCP only] Added option to enable PBS instance group auto-scaling.
  - [AWS only] Fixed AWS DynamoDB authentication return code.
  - [AWS only] Fixed AWS tarball name reference in our release script.
  - [AWS only] Updated PBS ELB monitoring by setting 4xx and 5xx error rate alarms to critical.

## [1.8.1](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.8.0...v1.8.1) (2024-06-14)

- **Important note**
  - To address a discovered PBS Auth issue please follow the steps described in the changes section to enable sites_as_authorized_domain.

### Changes

- Fixed authentication issue related to use of PrivacyBudgetService V2 API.
- To enable the use of adtech site as authorized domain perform the following steps:
  - [GCP]
    - In the `auto.tfvars` file of distributedpbs_application, in the `pbs_application_environment_variables` map, add the following lines
      ```
      {
       name = "google_scp_pbs_adtech_site_as_authorized_domain_enabled"
       # The flag toggles using adtech site as authorized domain.
       value = "true"
      },
      ```
  - [AWS]
    - In the `auto.tfvars` file of distributedpbs_application, in the `application_environment_variables` map, add the following line
      ```
      google_scp_pbs_adtech_site_as_authorized_domain_enabled = "true"
      ```
- Rollback steps:
  - Rollback the terraform changes performed in the above steps.
  - Deploy the previous release (v1.8.0-rc01) along with the rolled back terraform changes.

## [1.8.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.7.0...v1.8.0) (2024-05-14)

- **Important note**
  - [AWS only] Added new allowlist `allowed_principals_set` for services that only deploy
    MPKHS and do not use the PBS auth feature.`allowed_principals_set` is a list of AWS Account IDs from
    `allowed_principals_map_v2`. Migration from `allowed_principals_map_v2` to `allowed_principals_set` can be done during the same terraform plan/apply. Change the variable in your `<name>.auto.tfvars` from `allowed_principals_map_v2` to `allowed_principals_set` and convert the map to a list deleting the sites association

### Changes

- Deprecated single party key support in MPKHS
- Updated dependencies to address security vulnerabilities
- [AWS only] Added `attestation_pcr_allowlist` to replace `attestation_condition_keys` in Terraform input values to
  address the PCR0 list limitation per policy of 54 hashes
- [AWS only] Added policy to deny non-ssl requests for `pbs_elb_access_logs`
- [AWS only] Enforced IMDSv2 support on the EC2 instances that build the enclave AMIs
- [AWS only] Made changes to `roleprovider` which now takes either `allowed_principals_map_v2` or `allowed_principals_set`
  based on whether PBS is deployed for sites based authentication
- [AWS only] Switched from using `etag` to use `source_hash` for S3 stored deployment artifacts to address plan
  changes without artifact changes
- [GCP only] Added `allowed_wip_iam_principals` to replace `allowed_wip_user_group` in Terraform input variables for WIPP
  configuration; for MPKHS, a Google Group is no longer required to manage allowed coordinator users
- [GCP only] Addressed GCP build container build issue
- [GCP only] Fixed attestation case when `enable_attestation` is set to `false`
- [GCP only] Implemented improvements in PBS scaling

## [1.7.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.6.1...v1.7.0) (2024-04-09)

### Changes

- Added ReadJournalFile functionality to the journal reading tool
- Added support for caching exceptions from Private Key Service
- Adopted release candidates for building and pushing Coordinator Services artifacts
- Enabled support for workers to relinquish job ownership so that other available workers can pick it up
- Implemented GetBlobSize function in BlobStorageClient
- Updated container dependencies
- [GCP only] Added autoscaling and worker service account variables for GCP Aggregation Service deployment script
- [GCP only] Added variable to enable/disable load balancer logs for Public Key Hosting Service
- [GCP only] Enabled live migration for host maintenance on Aggregation Service
- [GCP only] Fixed keygen image path in the GCP build script
- [GCP only] Moved Container Digest/Reference allowlisting to IAM Bindings
- [GCP only] Updated GCP instance startup script so that PBS log is parsed correctly
- [Java CPIO only] Deprecated single party key support

## [1.6.1](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.6.0...v1.6.1) (2024-03-19)

- **Important note**
  - **[AWS only]** Cloudwatch alerting changes requires special handling in order to preserve log:
    - Prior to generating the Terraform plan, run the following command:
      `terraform import module.distributedpbs_application.module.auth_service.aws_cloudwatch_log_group.lambda_cloudwatch /aws/lambda/${environment}-google-scp-pbs-auth-lambda`
    - In case of rollback, prior to generate the Terraform plan, run the following:
      `terraform state rm module.distributedpbs_application.module.auth_service.aws_cloudwatch_log_group.lambda_cloudwatch`
  - **[GCP only]** GCP coordinator services is officially open sourced with this release.

### Changes

- Added api_gateway and lambda alarms for PBS Auth service
- Fixed a bug where PBS is not picking up the correct parameters for refreshing leader lease; it wwill refresh the lease more frequently (every 5s) after this fix
- Pinned Docker engine version to 24.0.5 for operator AMI build
- Switched PBS instance disk to regional SSD
- Updated container dependencies
- Updated logging in PBS so that only the first 5 journal IDs are logged
- Updated PBS instance startup script to increase the soft file descriptor limit
- [GCP only] Updated fluentd config for PBS so that PBS logs are parsed correctly before being sent to GCP Cloud Logging

## [1.6.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.5.1...v1.6.0) (2024-02-29)

- **Important note**
  - SSH access to PBS EC2 instances restricted to localhost only
  - Starting with this patch, terraform variables allowed_principals_map and allowed_principals are no longer supported and need to be removed from the tfvars file in roleprovider environment.
  - **[AWS only]** Possible Action Required in primary coordinator: \
    This release includes terraform change for adding dynamodb:DeleteItem permission in the multiparty key generation service.
    It possibly requires more permission in terraform deployment role.

### Changes

- Added dynamodb:DeleteItem permission to key generation service instance
- Added exception handlers in GcsBlobStorageClient
- Added logic to clean up temp key and retry when key generation fails
- Added probers for coordinator services
- Added validation around 'job_request_id' parameter of the GetJob API
- Changed attribute mapping in WIPP to remove token creation limitation
- Created an autoscaling hook so worker instances can be updated without interrupting current running jobs
- Disallowed unencrypted S3 data transports
- Enabled flexibility in tuning individual MPKHS and PBS alarms independently
- [Java CPIO library] Increased private key cache TTL to 8 hours for Java client
- Made MPKHS prober alarm service label more descriptive
- Migrated operator IAM roles to be created using allowed_principals_map_v2 and removed the now deprecated allowed_principals_map
- Removed single coordinator PBS code and associated tests
- Set prod confidential space image as the default
- Updated container dependencies
- Updated PBS client to support the PBS APIv2 schema
- Upgraded PBS Client library to support PBS API changes for supporting budget consumption of budget keys belonging to multiple reporting origins

## [1.5.1](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.5.0...v1.5.1) (2023-12-12)

- **Important note**
  - SSH access to PBS EC2 instances now restricted to localhost only
  - Starting with this patch, terraform variables allowed_principals_map and allowed_principals are no longer supported and need to be removed from the tfvars file in roleprovider environment

### Changes

- Limited SSH access to PBS EC2 instances from localhost only
- Changed to generate operator IAM roles based on new allowed_principals_map_v2 map
- Added necessary tools (python3-venv & zip) to install during the build container Docker build process
- Updated container dependencies
- Fixed GCP alarm window functions

## [1.5.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.4.0...v1.5.0) (2023-11-14)

- **Important note**
  - To set the stage for enabling per-site enrollment on AWS:
    - In the `auto.tfvars` file of distributedpbs_application, in the `application_environment_variables` map, add the following line
      ```
      google_scp_pbs_authorization_enable_site_based_authorization = "false"
      ```
  - Rollback steps:
    - Rollback the terraform changes performed in the above steps.
    - Deploy the older code (v1.4.0) along with the rolled back terraform changes.

### Changes

- Added per-site enrollment feature behind a feature flag
- Allowlisted remote coordinator's site information in the new auth table as part of deployment
- Removed UnifiedKeyHostingApiGatewayTotalErrorRatioHigh alarm
- Enabled userArn context in api gateway logging
- Enabled point in time recovery for the partition lock table
- Updated AWS authorizer lambda to always force https protocol on reporting origin from requests
- Updated default alerting window to 5 minutes for KHS alarms

## [1.4.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.3.0...v1.4.0) (2023-10-11)

- **Important note**
  - To enable batching of metrics, the following line needs to be added to the auto.tfvars of the distributedpbs_application in the application_environment_variables variable map.
    ```
    application_environment_variables = {
      google_scp_pbs_metrics_batch_push_enabled = "true"
    }
    ```
  - To be compatible to the existing Aggregation service versions, the following line needs to be added to the auto.tfvars of mpkhs_primary and mpkhs_secondary
    ```
    get_encryption_key_lambda_ps_client_shim_enabled = true
    ```

### Changes

- Added support for Google Cloud Platform (GCP)
- Added new API endpoint to serve public keys in the Key Hosting Service
- Added use of S3TransferManager to support multi-threaded uploads for BlobStorageClient
- Added sending notifications when alarms transition to ok state
- Disabled GCP CDN logs to avoid logging sensitive information
- Updated Guava library version to v32.0.1
- Updated Jackson library version to v2.15.2
- Updated Java base to the latest version as of 09/18/2023
- Updated Amazon Linux 2 to v2.0.20230912
- Addressed bug in PBS checkpoint service thread
- Addressed race condition between Stop and Transactions entering the Transaction Manager
- Addressed Journal Recovery bug during Checkpointing
- Addressed VNode Lease Count fluctuation
- Addressed NgHttp2 callback related crashes in Http2Server
- Addressed losing kLeaseAcquired event when re-acquring Lease immediately
- Addressed issue in BudgetKeyProvider's Stop()

## [1.3.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.2.0...v1.3.0) (2023-09-27)

### Changes

- Set finishedAt field when appending job metadata error message
- Disabled warnings about unencrypted SNS topics
- Renamed all alerts to follow the new naming format
- Added API version to MPKHS public key serving path
- Added new path for MPKHS to serve public keys
- Added the ability to support multiple domains for the public key endpoint
- Added ProcessingStartTime in Job Object
- Added OTel metrics control in terraform
- Updated PBS elasticbeanstalk configuration to ignore 4xx app and lb errors during health reporting

## [1.2.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.1.1...v1.2.0) (2023-08-24)

### Changes

- Fixed terraform plan output
- Extended operator lifecycle hook timeout
- Added Codebuild terraform and instructions

## [1.1.1](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.1.0...v1.1.1) (2023-08-08)

### Changes

- Ensured ACL is set on CloudFront logs bucket by adding an explicit dependency
- Added option to uninstall SSH server from AMI
- Pinned and locked versions for nsm-api dependencies
- Enforced newer SSLPolicy on ELB LB

## [1.1.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.0.0...v1.1.0) (2023-07-27)

### Changes

- Fixed Alert-Queue by updating region
- Fixed kmstool build by upgrading rust toolchain to 1.63 and pinning the nsm-api dependencies
- Added retry with exponential back off for decryption service
- Added new error code to map private key endpoint
- Improved ConstantJobClient usability for testing

## [1.0.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v0.51.15...v1.0.0) (2023-07-10)

### Changes

- Release of first major version v1.0.0 including open sourcing of coordinator services source code
