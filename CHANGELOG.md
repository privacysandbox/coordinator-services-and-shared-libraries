# Changelog
## [1.29.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.28.0...v1.29.0) (2025-07-21)
### Important Note
[GCP]
- [Default backup schedules](https://cloud.google.com/spanner/docs/backup#default-backup-schedules) will no longer be created on new KeyDB databases
- [Feature enabling] Custom backup schedules can be set for KeyDB
  - [To Enable] Configure the `key_db_backups` variable in `mpkhs_primary` or `mpkhs_secondary`:
    ```
      # Example custom backup schedule for KeyDB: 90d retention, daily full backup at 00:00 UTC
      key_db_backups = {
        retention_duration = "7776000s"
        cron_spec          = "0 0 * * *"
        incremental        = false
      }
    ```
  - [To Rollback] Remove the `key_db_backups` variable.
- [Feature enabling] A log-based metric can be created for scheduled Spanner backup events
  - [To Enable] Configure the `enabled_logging_metrics` variable in `mpkhs_primary` or `mpkhs_secondary`:
    ```
      enabled_logging_metrics = {
        spanner_scheduled_backups = true
      }
    ```
  - [To Rollback] Remove the `enabled_logging_metrics` variable.

### Changes
- INFRA
  - [GCP] Update security policies to use optional attributes
  - [GCP] Use write-only attribute alternative in parameters module
- MPKGDS
  - [GCP] Add a log-based metric for Spanner backup events
  - [GCP] Allow setting a Spanner backup schedule
  - [GCP] Remove dependency on KHS jars after completing migration from Cloud Function to Cloud Run
- BUILD
  - [CA] Update container dependencies
- PBS
  - [CA] Extract integration tests


## [1.28.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.27.0...v1.28.0) (2025-07-07)
### Important notes
[GCP]
- The shared/ directory found under `coordinator/terraform/gcp/environments_mp_(primary|secondary)` has been removed.

[CA]
- Infastructure has been upgraded to use Terraform version v1.12.1. Please upgrade to Terraform version 1.12.X before executing any terraform commands. Terraform code will only be backwards compatible with v1.2.3 for this release only.

### Changes
- INFRA
  - [GCP] Remove `shared/` terraform directory
- MPKGDS
  - [GCP] Add the legacy AWS public key path to GCP request handler map to
    support old version of Chrome client
  - [GCP] Allow specifying KeyDB name suffix
  - [GCP] Enable specifying Spanner edition
- BUILD
  - [CA] Update container dependencies
- PBS
  - [CA] Cleanup C++ uses of
    `google_scp_pbs_migration_enable_budget_consumer_migration`
  - [CA] Clean cout statements in http2 connection pool test
  - [GCP] Allow specifying database name suffixes
  - [GCP] Enable specifying Spanner instance config and edition
  - [GCP] Removed unused VPC from Terraform

## [1.27.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.26.0...v1.27.0) (2025-06-24)
### Important notes
[GCP]
- `enable_key_generation` is set to `true` by default.
- [Breaking change] The `project_id` variable has been removed from the `allowedoperatorgroup` module.
- KeyDb schema is now managed by Liquibase. Please see `./coordinator/db/liquibase.sh` for reference.
  - Before the release, `./liquibase.sh changelog-sync-to-tag 1` **must** be run to intialize the database.
  - Before terraform apply, `./liquibase.sh update` **must** be run. The update will be a no-op, as no new changes are introduced in this release.

### Changes
- MPKGDS
  - [GCP] Make defaults in application module consistent
  - [GCP] Remove unused variables in terraform configuration of `allowedgroupoperator`,`multipartykeyhosting_secondary`, `operator_workloadidentitypoolprovider`
  - [GCP] Set keygen MIG to BALANCED shape
- BUILD
  - [CA] Remove non-bzlmod support
  - [CA] Update container dependencies.
- PBS
  - [CA] Cleanup `google_pbs_stop_serving_v1_request flag` in PBS codebase
  - [GCP] Remove references to PBS VM image in build scripts
  - [GCP] Remove unused variables in terraform configuration of `distributedpbs_alarms`, `distributedpbs_application`, `distributedpbs_base`

## [1.26.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.25.0...v1.26.0) (2025-06-10)
### Changes
- INFRA
  - [CA] Remove terraform version constraint in child modules
- MPKGDS
  - [AWS] Fix AWS assume role policy issue with multiple roles
  - [GCP] Ignore changes to KeyDb DDL Terraform field
- PBS
  - [CA] Introduce docker compose binary
  - [CA] Introduce test to verify one to one mapping between budget type and budget consumer
  - [CA] Use docker-compose in PBS GCP Integration test

## [1.25.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.24.0...v1.25.0) (2025-05-27)
### Important Note
**[GCP]**
- Provider definitions have been removed from PBS Terraform. Ensure that the default project and region are set appropriately at the root module level
- [Feature enabling] Update PBS to stop serving v1 request format.
  - [To Enable] The feature is enabled by default. No action is required.
  - [To Rollback] To rollback, add the following variable in the `auto.tfvars` for
    `distributedpbs_application`:

    ```
    pbs_application_environment_variables = [
      ...
      {
        name = "google_pbs_stop_serving_v1_request"
        value = "false"
      },
    ]
    ```
- [Feature enabling] Option to enable security policies and adaptive protection on external load balancers for protection against DDoS attacks.
  - [To Enable] Prior to enabling the Adaptive Protection feature flag, the billing account and project both need to be subscribed to Cloud Armor Enterprise.
    - In the `auto.tfvars` of each service you want to enable:
      - Set the flag `enable_security_policy` to `true`.
      - Set the flag `use_adaptive_protection` to `true`.
    - In the `auto.tfvars` for `distributedpbs_application`, define rules for PBS using the variable `pbs_security_policy_rules`
    - In the `auto.tfvars` for `mpkhs`, define rules for each service using the following variables:
      - Primary Coordinator:
        - `public_key_security_policy_rules`
        - `encryption_key_security_policy_rules`
      - Secondary Coordinator:
        - `encryption_key_security_policy_rules`
        - `key_storage_security_policy_rules`
    - In the `auto.tfvars` of each service with DoS protection enabled that you want alerts for:
      - Set the flag `enable_cloud_armor_alerts` to `true`.
      - If you also want notifications sent for alerts:
        - Set the flag `enable_cloud_armor_notifications` to `true`.
        - Set the flag `cloud_armor_notification_channel_id` to a valid `google_monitoring_notification_channel` resource id.
    - Optionally configure the Adaptive Protection detection thresholds. In the `auto.tfvars` of each service you want to configure, define thresholds using the following variables:
      - `pbs_ddos_thresholds`
      - `public_key_ddos_thresholds`
      - `encryption_key_ddos_thresholds`
      - `key_storage_ddos_thresholds`
  - [To Rollback] Remove or set the flags `enable_security_policy`, `use_adaptive_protection`, and `enable_cloud_armor_alerts` to `false`.
- [Feature enabling] Create a certificate manager certificate for Public Key default domain
  - [To Enable] The feature is enabled by default. No action is required.
  - [To Rollback] To rollback, run the following cmd to remove the certificate map from the target proxy.
    ```
    gcloud compute target-https-proxies update \
    a-public-key-service-cloud-run --clear-certificate-map \
    --project <gcp_project_name>
    ```

### Changes
- BUILD
  - [CA] Update container dependencies
- INFRA
  - [GCP] Add Cloud Armor adaptive protection threshold configs
  - [GCP] Remove missing output vars in demo environment
- MPKGDS
  - [GCP] Add configuration for Public key DNS cutover.
  - [GCP] Create cert manager cert for the public key alternative domains, this is put behind a flag.
  - [GCP] Migrate public key cert from compute SSL cert to cert manager managed cert to prepare for DNS migration
  - [GCP] Remove the dependency of public key ssl cert in targe proxy with one flag, and clean up the ssl cert with another flag
  - [GCP] Traffic splitting is now available for Public Key Service and Private Key Service. The variable `traffic_percent` can be configured to define the proportion of traffic routed to the revision with the updated configuration. When omitted, this variable defaults to 100%, indicating no traffic splitting occurs.
- PBS
  - [CA] Introduce SyncHttpClient
  - [CA] Complete Binary Budget Consumer test in new GCP post-submit test
  - [CA] Flip `google_pbs_stop_serving_v1_request` to true
  - [CA] Introduce new post-submit framework for PBS
  - [CA] Propagate `certificate_manager_has_prefix` variable to pbs application
  - [CA] Update cert manager naming so that they are prefixed with env name
  - [GCP] Add `project_id` output value to `distributedpbs_base`
  - [GCP] Remove `region` variable from the `distributedpbs_alarms` terraform module
  - [GCP] Remove provider definitions from PBS modules

## [1.24.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.23.0...v1.24.0) (2025-05-13)
### Important Note
**[GCP]**
- [Breaking change] Before deployment, please remove the following variables from the auto.tfvars for distributedpbs_application
    ```
    pbs_cloud_run_traffic_percentage = 100
    deploy_pbs_cloud_run             = true
    enable_pbs_cloud_run             = true
    machine_type                     = "n2-standard-32"
    root_volume_size_gb              = "512"
    pbs_cloud_logging_enabled        = true
    pbs_cloud_monitoring_enabled     = true
    pbs_instance_allow_ssh           = false
    enable_public_ip_address         = false
    pbs_custom_vm_tags               = ["allow-health-checks"]
    ```
  - [To Rollback] Restore previous version of terraform.

### Changes
- INFRA
  - [GCP] Refactor Cloud Armor security policy into shared module
  - [GCP] Update coordinator demo environment to remove dependency on 'shared' directory
- MPKGDS
  - [GCP] Add Cloud Armor alerting policies for KMS.
  - [GCP] Prepare initial Liquibase changeset for KeyDb (for future adoption)
  - [GCP] Refactor SpannerKeyDbConfig to generic SpannerDatabaseConfig
  - [GCP] Remove unused KeyDb readStaleness parameter
  - [GCP] Replace SpannerMetadataDbConfig with generic SpannerDatabaseConfig
  - [GCP] Simplify method names of SpannerDatabaseConfig
  - [GCP] Simplify SpannerEmulatorContainerTestModule by passing config instead of individual args
- OPERATOR
  - [AWS] Delete AWS EBS volume on instance termination
- BUILD
  - [CA] Update container dependencies.
- PBS
  - [CA] Change config provider to use absl::StrSplit instead of SplitStringByDelimiter
  - [CA] Change PeriodicClosure default to use real clock
  - [CA] Clean up long namespaces that are specified in the //cc/...
  - [CA] Delete unused codes under cc/core/utils
  - [CA] Delete use of "cc/core/common/proto/common.proto" from C++ code
  - [CA] Move "cc/core/utils" folders to privacy_sandbox::pbs_common namespace
  - [CA] Move "cc/public/core" and telemetry codes to to privacy_sandbox::pbs_common namespace
  - [CA] Move google::scp::pbs to the privacy_sandbox::pbs namespace
  - [GCP] Add Cloud Armor alerting policies for PBS.
  - [GCP] Delete GCE PBS terraform. All traffic is already being served by Cloud Run PBS
  - [GCP] Unconditionally deploy Cloud Run PBS. Deprecate related tfvars.

## [1.23.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.22.0...v1.23.0) (2025-04-29)
### Important Note
**[GCP]**
- Update the GCP Terraform provider version to 6.29.0.
  - [To Enable] Update the Terraform dependency lock file (`.terraform.lock.hcl`) by executing the below command:
    ```
    terraform init -upgrade
    ```
  - [To Rollback] Restore previous version of dependency lock file.
- [Feature enabling] The `google_scp_pbs_enable_request_response_proto_migration` flag acts as a feature toggle to control whether the PBS Server should attempt to use `ConsumePrivacyBudgetRequest` and `ConsumePrivacyBudgetResponse` protocol buffers for parsing JSON request and serializing response bodies.
  - [To Enable] The feature is enabled by default. No action is required.
  - [To Rollback] To rollback, add the following variable in the `auto.tfvars` for `distributedpbs_application`:
    ```
    pbs_application_environment_variables = [
      ...
      {
        name = "google_scp_pbs_enable_request_response_proto_migration"
        value = "false"
      },
    ]
    ```

### Changes
- BUILD
  - [AWS] Update `glibc` packages version to `2.26-64.amzn2.0.4` in `amazonlinux_2` container dependency
  - Replace deprecated `@rules_java//java:defs.bzl` with `@com_google_protobuf//bazel:java_proto_library.bzl`
- MPKGDS
  - [GCP] Update the GCP Terraform provider version to `6.29.0` for all modules
- PBS
  - [GCP] Detach GCE PBS from PBS Load Balancer
  - [GCP] Enable Request Response protos in PBS Server
  - [GCP] Introduce API proto in Integration test
  - [GCP] Remove PBS image from the built coordinator tarball
  - [GCP] Update Terraform PBS config so child templates can override `environment_variables` variables from base templates
  - [GCP] Update the GCP Terraform provider version to `6.29.0` for all modules
  - Add `json_name` for each field in api proto to avoid serialization issues
  - Implement proto parsing in binary budget consumer
  - Introduce `ParseCommonV2TransactionRequestProto` in front end utils
  - Introduce the `ConsumePrivacyBudgetRequest` and `ConsumePrivacyBudgetResponse` proto
  - Move `Config Provider` and `cc/core/common` folders to `privacy_sandbox::pbs_common` namespace
  - Remove unused code
  - Stop serving disabled v1 request in PBS
  - Update PBS API proto to use [API-specific protos](https://google.aip.dev/215)

## [1.22.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.21.0...v1.22.0) (2025-04-15)
### Important Note
**[GCP]**
- [Cleanup] Cleanup the terraform file as Cloud Function to Cloud Run migration is done
  - [To Cleanup] Coordinators must be on version 1.21 to perform this action. Remove the following variables from `.tfvars` for MPKHS if they are present to decommission Cloud Functions:
    - Primary Coordinator:
      - `use_cloud_run`
      - `get_public_key_service_zip`
      - `encryption_key_service_zip`
      - `mpkhs_package_bucket_location`
    - Secondary Coordinator:
      - `use_cloud_run`
      - `encryption_key_service_zip`
      - `key_storage_service_zip`
      - `mpkhs_package_bucket_location`
      - `alarms_enabled`
      - `alarms_notification_email`
  - [To Rollback] Revert the changes in terraform files and deploy

- [Breaking change] Remove PBS deployment scripts
  - In the `auto.tfvars` for `distributedpbs_application` please add:
  ```
    pbs_image_override = <url_to_privacy_budget_service_image>
    pbs_service_account_email = "<output from distributedpbs_base>"
  ```
  - [To Rollback] Revert the flags and rollback to the previous version

### Changes
- BUILD
  - Update container dependencies
- MPKGDS
  - [GCP] Add security policies for KMS external load balancers
  - [GCP] Cleanup terraform output with the completion of Cloud Function to Cloud Run migration
- PBS
  - [GCP] Add security policy for PBS external load balancer
  - [GCP] Remove decommissioned PBS deployment scripts
  - [GCP] Remove unused PBS environment variables from PBS binary, but not from terraform configs
  - [GCP] Update PBS journal bucket lifecycle rule to delete all data in the bucket
  - Add `Wthread-safety` to `periodic_closure`
  - Add startup delay to `periodic_closure`
  - Simulated clock in `periodic_closure`



## [1.21.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.20.0...v1.21.0) (2025-04-01)

### Important Note
**[GCP]**
- [Feature enabling] We have introduced a new BudgetConsumer based design that allows easy integration of new budget consumer variants.
  - [To Enable] In the `auto.tfvars` for `distributedpbs_application`, add the following variables:
    ```
    pbs_application_environment_variables = [
      ...
      {
        name  = "google_scp_pbs_migration_enable_budget_consumer_migration"
        value = "true"
      },
    ]
    ```
  - [To Rollback]Set this flag to `false` and re-deploy PBS in the current version.

### Changes
- BUILD
  - Update container dependencies
- MPKGDS
  - [GCP] Bump version of Spanner emulator image used in tests
  - [GCP] Detach the temporary ssl certificates from Public Key Service and Private Key Service
- PBS
  - Fix http2_server_test flakiness
  - Format for binary_budget_consumer
  - Introduce Budget consumer interface and binary budget consumer
  - Introduce ParseCommonV2TransactionRequestBody which separates out the common v2 PBS HTTP API parsing
  - Introduce the ConsumePrivacyBudgetRequest and ConsumePrivacyBudgetResponse proto
  - Move logger to pbs_common namespace
  - Refractor ConsumeBudget to support modular design
  - Refractor FrontEndUtils and introducing modular design in FrontEnd service only
  - Remove usage of deploy_distributedpbs.sh

## [1.20.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.19.0...v1.20.0) (2025-03-18)

### Important Note
- Continue to use the bazel WORKSPACE
  - The following steps are only required if you are unable to use bzlmod for dependency management.
  - If your repository is depending on `javatests/com/google/scp/shared/testutils/gcp/CloudFunctionEmulatorContainer.java` you must patch `CloudFunctionEmulatorContainer.java` so that the value of `invokerJarPath` is `external/maven/v1/https/repo1.maven.org/maven2/com/google/cloud/functions/invoker/java-function-invoker/1.1.0/`.
  - To check if you are depending on `CloudFunctionEmulatorContainer.java`, you may use the following command `bazel query 'allpaths(//path/to/your/target,//javatests/com/google/scp/shared/testutils/gcp:gcp)'`

### Changes
  - Add budget_value Golang library
  - Add deprecation warning message to PBS deploy scripts
  - Add `PeriodicClosure` util
  - Code cleanup by removing frontend `FrontEndUtils::CreateMetricLabelsKV` and `google.scp.pbs.requests` metrics, unused code under `//cc/core/…` and `cc/core/authorization_service/…`, pruning `envoy_api` dependencies
  - Enable bzlmod for Coordinator Services
  - Update Key Migration Tool to support xCC key migration use case.
  - Update visibility rules for `cc/public/…`
  - [AWS only] Update test size for java tests that depend on LocalStackContainers
  - [GCP only] Add build of PBS image as part of GCP Cloud Build config
  - [GCP only] Add `image_params.auto.tfvars` for mpkhs_secondary for Cloud Run container locations
  - [GCP only] Add option to bypass terraform output lookups
  - [GCP only] Add private and public key service images variables to `image_params.auto.tfvars` for mpkhs_primary
  - [GCP only] Add `service.name` to KHS OTel resource attribute
  - [GCP only] Disable external ingress for `pbs_instance` Cloud Run
  - [GCP only] Set `cpu_idle` and `startup_cpu_boost` for MPKHS services Cloud Run
  - [GCP only] Turn on KHS OTel metrics by default
  - [GCP only] Remove flag `google_scp_migrate_http_status_code` after successful migration.

## [1.19.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.18.0...v1.19.0) (2025-03-04)

### Changes
  - Added Terraform option to enable / disable key rotation.
  - Changed `KGDS (Key Generation and Distribution Service)` to persist the correct key split data in `keydb` database under key sync mode
  - Fixed having a non-trivially destructible static variable in PBS frontend service
  - Propagate auth errors from `PrivacyBudgetClientImplV2` to `TransactionOrchestratorImpl`
  - Refactored `OperatorClientConfig` to make `coordinator_a_wip_provider` and `coordinator_a_sa` optional for `AWS_TO_GCP` client
  - Replaced deprecated `@rules_proto` with `@com_google_protobuf`
  - Upgrade PBS image to use debian 12
  - Updated root directory of `cpplint`
  - Updated `amazonlinux` container version
  - Updated `debian` container version
  - [AWS only] Unset `desired_capacity` attribute from `aws_autoscaling_group` `split_key_rotation_group` resource in `multipartykeygenerationservice` module
  - [GCP only] Fixed Key Storage Service `CreateKeyTask` binding issue when `AWS_KEY_SYNC_ENABLED=true`
  - [GCP only] Granted `iam.serviceAccountOpenIdTokenCreator` role to the verified service account

## [1.18.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.17.0...v1.18.0) (2025-02-18)

### Important Note
- [GCP only] Configure `phase_2` in the PBS Spanner migration and enable writes to the ValueProto column.
  - In the `auto.tfvars` for `distributedpbs_application`, add the following variables:
    ```
    pbs_application_environment_variables = [
      ...
      {
        name  = "google_scp_pbs_value_proto_migration_phase"
        value = "phase_2"
      },
    ]
    ```
  - This terraform configuration variable is needed to enable writing the `ValueProto` column along with the `Value` column. This is the next step in migrating PBS Spanner to use the `ValueProto` column instead of the `Value` column.
  - Quick description of the PBS Spanner migration phases:
    - `phase_1` : Writes and reads from the `Value` column only. The `ValueProto` column is ignored. This is the default state.
    - `phase_2` : Writes both `ValueProto` and `Value` column but reads from `Value` column only.
    - `phase_3` : Writes both `ValueProto` and `Value` column but reads from `ValueProto` column only.
    - `phase_4` : Writes and reads from `ValueProto` column only. The `Value` column is ignored.
  - Rollback instructions: Rollback the terraform changes performed in the above steps and deploy PBS in the current version.
    - Default value of `google_scp_pbs_value_proto_migration_phase` is `phase_1` in which PBS neither writes nor reads the `ValueProto` column.
    - Rolling back this environment variable in `phase_1` should sufficiently restore PBS to it's previous state.
- [GCP only] Configure MPKHS services running on Cloud Run.
  - Note that MPKHS will continue to use Cloud Functions. Cloud Run services will be provisioned alongside, but will not be serving.
  - For Primary Coordinator:
    ```
    use_cloud_run                        = false
    public_key_service_image             = <url_to_public_key_service_image>
    private_key_service_image            = <url_to_private_key_service_image>
    private_key_service_custom_audiences = [
      ...,
    ]
    ```
  - For Secondary Coordinator:
    ```
    use_cloud_run                        = false
    private_key_service_image            = <url_to_private_key_service_image>
    key_storage_service_image            = <url_to_key_storage_service_image>
    private_key_service_custom_audiences = [
      ...,
    ]
    key_storage_service_custom_audiences = [
      ...,
    ]
    ```

### Changes

- Removed unused code from PBS after support of GCP only deployment
- Migrated part of external dependencies to `MODULE.bazel`
- Removed nodejs dependencies in `WORKSPACE.bzlmod`
- Removed unused `get_encrypted_private_key_request.proto` in MPKHS
- Set `--test_output=errors` option for bazel build command
- Updated `CC_BUILD_CONTAINER_REGISTRY` to point to new location for hosted images
- Updated Google Cloud dependencies
- Updated bazel version to `7.5.0` without enabling bzlmod
- Updated container dependencies
- Updated content of bzlmod file
- Updated import of `cc_proto_library` to import the rules from `com_google_protobuf`
- Updated visibility rules for most packages under `cc/...`
- [GCP only] Added files for managing `KeyDb` DDL statements
- [GCP only] Added option for syncing keys to another pair of Coordinators during the key generation process to support migrations
- [GCP only] Disabled MPKHS cloud functions deployment when `_package_zip` terraform variable is not provided
- [GCP only] Guarded removal of AWS xCC KMS keys with `prevent_destroy`
- [GCP only] Prepared MPKHS cloud run migration

## [1.17.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.16.0...v1.17.0) (2025-02-04)

### Changes

- Added the `envoy_api` dependency for PBS API rate limiting
- Added `initialize_distributedpbs.sh` script
- Added job success and fail metrics to GCP and AWS operator terraform
- Added the `-t` flag to the docker command when building a Bazel target within a container
- Fixed address sanitizer issue in `test_http1_server.cc`
- Fixed failing target after enable `--incompatible_disallow_empty_glob`
- Integrated with `asan`, `ubsan`, `tsan`, `msan` sanitizers in PBS
- Modified to `pbs_value_column` for better variable names and removing unnecessary dependencies
- Moved `libpsl` target to its own repo
- Removed unused/outdated PBS code from core components and tests
- Replaced `@nlohmann_json//:lib` with `@nlohmann_json//:singleheader-json`
- Update import of `cc_proto_library`, `cc_grpc_library`, `java_grpc_library` so that it is compatible with the rule define in bazel central repository
- Updated `go.work` so that it is in root directory
- Updated container dependencies
- Updated integration test for PBS (build with local flag, so that pbs interacts with local spanner) with Spanner emulator
- Updated link to `spanner_backup`
- Updated rules naming to `com_github_curl_curl` and `com_github_nlohmann_json`
- Updated visibility rule for all libraries belong to PBS
- Upgraded `Guava` to `33.3.1-jre` version
- Upgraded `abseil` to `20240722.1` version
- Upgraded `io_grpc_grpc_java` to `1.56` version
- Upgraded `opentelemetry-cpp` to `1.17.0` version and `.bazelrc` so that OTel `copts` flags are not propagated when building non-OTel libraries
- Upgraded `rules_cc` to `0.0.17` version
- Upgraded `rules_jvm_external` to `6.6` version
- Upgraded `rules_python` to `0.39.0` version and remove unused `privacy_ml` python
- [AWS only] Refactored default Terraform xcc variables
- [AWS only] Updated `pbs_heartbeat.py` to probe `:prepare` endpoint
- [AWS only] Updated `reproducible_proxy_outputs` to eliminate reliance on the `/tmp` directory
- [AWS only] Upgraded `glibc` libraries to `2.26-64.amzn2.0.3` version to be compatible with updated default `glibc-common` version
- [GCP only] Added back the `operator_wipp:operator_wipp_demo` file to the tar, which was deleted by accident
- [GCP only] Added config parameters to support writing and reading from the `ValueProto` column
- [GCP only] Added missing deploy permissions for alarm setup on operator
- [GCP only] Added support for writing `ValueProto` Column with `LaplaceDP Budgets` only
- [GCP only] Added support to read `ValueProto` column in `phase 3` and `phase 4`
- [GCP only] Combined `ReadPrivacyBudgetsForKeys` and `CreatePbsMutations` to hide underlying type
- [GCP only] Inlined Container VM Metadata module for PBS instance
- [GCP only] Propagated default Terraform variable values for PBS from `shared/` to `applications/`
- [GCP only] Reformed distributedpbs `README.md` files and the `instance_startup.sh` file was renamed to `instance_startup.sh.tftpl`
- [GCP only] Simplified `toCloudFunctionResponse` by eliminating redundant code
- [GCP only] Updated PBS `auth_cloud_function_handler_path` to be nullable
- [GCP only] Updated `pbs_image_tag` to be nullable
- [GCP only] Updated default Terraform variables values for PBS `pbs_cloud_run_max_instances` and `pbs_cloud_run_min_instances`

## [1.16.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.15.0...v1.16.0) (2025-01-21)

### Changes

- A dedicated exception, `ObjectConversionException`, was created to be thrown when an error occurred during the conversion of an object to another type
- Created trace fakes for in-memory integration testing
- GCP end-to-end test bazel dependencies were separated by removing unnecessary AWS dependencies
- Installed the required `glibc-common` version in the `reproducible_proxy_outputs` bazel target
- Modified `RecordServerLatency` and `RecordRequestBodySize` to take `Http2SynchronizationContext` and `AsyncContext`, respectively, as constant references
- Removed MetricClient from `//cc/pbs/front_end*`  and `//cc/core/...`
- Setup Trace SDK
- Some HTTP 4xx return codes were updated to the correct 5xx codes
- The GCP Key Storage Service renamed `KmsKeyAead` to `DecryptionAead`
- The GCP Key Storage Service utilized dedicated `EncryptionAead`
- The `GcpKeyGenerationUtil` was refactored and moved to shared
- The usage of terraform default empty string input variables was replaced with null values
- Updated StdoutLogProvider to initialize JSON using an initializer list
- Upgraded `cc_rules` to the 0.0.9 version
- Upgraded `google-cloud-cpp` to 2.34.0 version
- Upgraded `java_rules` to the 7.12.2 version
- Upgraded `proto` to the 28.3 version
- [AWS Only] Removal of unused resources after launching cross-cloud PBS
    - With the launch of Cross-Cloud PBS, several AWS-specific Terraform modules are no longer required and have been removed from the `distributedpbs_application` module. This includes modules for:
        - PBS VPC (vpc)
        - Remote PBS Access Policy (`access_policy`)
        - PBS DynamoDB Storage (`storage`)
        - PBS Beanstalk Storage (`beanstalk_storage`)
        - In addition, most resources in the PBS Beanstalk Environment (`beanstalk_environment`) module have been removed, except the Route53 resource that is responsible for routing requests from AWS PBS to GCP PBS. Deployment script has been updated to stop pushing new PBS Docker image to AWS ECR.
    - Existing deployments using these modules will have the corresponding resources automatically removed upon deployment of this release. No manual action is required from coordinator operators.
- [AWS only] Removed unused `distributedpbs_access_policy`, `distributedpbs_alarms`, `distributedpbs_beanstalk_storage`, `distributedpbs_storage` Terraform modules
- [AWS only] The `aws_keyencryptionkey` resource attributes were updated to use hyphens instead of underscores
- [GCP only] Added `version.txt` symlink for terraform
- [GCP only] Added the ability to run MPKHS with AWS worker support. The feature requires deploying the `xc_resources_aws` app and adjusting the MPKHS configuration. It is disabled by default and does not affect existing users
- [GCP only] Modified Cloud Build command to produce the artifacts required for deploying MPKHS on Cloud Run. This update specifically includes generating the necessary container images for deploying the Public Key Service, Private Key Service, and Key Storage Service on Cloud Run.
- [GCP only] Introduced the flexibility to deploy MPKHS on either Cloud Run or Cloud Functions. By default, MPKHS will be deployed on Cloud Functions. To switch to a Cloud Run deployment, you can add the following configuration to your deployment settings:

  Primary Coordinator:
  ```
  use_cloud_run                        = true
  public_key_service_image             = <url_to_public_key_service_image>
  private_key_service_image            = <url_to_private_key_service_image>
  private_key_service_custom_audiences = [...]
  ```

  Secondary Coordinator:
  ```
  use_cloud_run                        = true
  private_key_service_image            = <url_to_private_key_service_image>
  key_storage_service_image            = <url_to_key_storage_service_image>
  private_key_service_custom_audiences = [...]
  key_storage_service_custom_audiences = [...]
  ```
- [Java CPIO library] Added a module that enables federation from AWS to GCP. This module is disabled by default and allows for future integration of AWS workers with GCP-based MPKHS

- [GCP only] 4xx error code migration
  - In the `auto.tfvars` for `distributedpbs_application`, add the following variables:
    ```
    google_scp_migrate_http_status_code = true
    ```
  - This terraform configuration variable is needed to enable the error code migration from 4xx to 5xx for certain errors. This addresses a few 4xx error codes which were incorrectly handled.
  - We identified that some errors classified as 4xx were incorrectly categorized and should have been 5xx. This issue was observed in the auto expiry concurrent map, primarily used for authorization, and in the HTTP client when PBS makes HTTP call to auth service.
  - To rollback, set `google_scp_migrate_http_status_code = false` and deploy PBS

## [1.15.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.14.0...v1.15.0) (2025-01-07)

### Important Note
- PBS no longer reads the following environment variables. If provided during deployment, these environment variables will be ignored by PBS.
  - `google_scp_pbs_vnode_lock_table_name`
  - `google_scp_pbs_vnode_lease_duration_in_seconds`
- **[GCP only]** Migration of HTTP status codes from the 4xx series to the 5xx series is being introduced. This behavior is controlled by the feature flag `google_scp_migrate_http_status_code`, which is set to `false` by default. This default setting ensures that the current behavior remains unchanged unless explicitly enabled.

### Changes

- Added build/include_order into cpplint filter
- Added validations for fields "attribution_report_to", "reporting_site" and "input_report_count" from job_parameters json object
- Cleanup of rules in pbs_container_multi_stage_build_tools.bzl
- Enabled instance based billing for Cloud Run
- Improved performance of DeserializeHourTokensInTimeGroup, Uuid ToString method, ParseBeginTransactionRequestBodyV2 and ExtractUserAgent
- Removed multi partition code
- Removed obsolete HTTP server routing code
- Updated rules_boost patch with mirror for boost_1_79_0
- [AWS only] Added worker role names in terraform output
- [AWS only] Patched aws_nitro_enclave_sdk_c with git shallow clone to reduce AMI build time
- [GCP only] Added an option to override PBS Auth GCS bucket and object name
- [GCP only] Added environment scope to GCP keygen autoscaler resource
- [GCP only] Added variables to configure concurrency and cpu allocation for Private Key Service
- [GCP only] Addressed terraform permadiff for cloud run
- [GCP only] Configured Cloud Build to store logs in Cloud Logging
- [GCP only] Copied public key default variable values to applications
- [GCP only] Migrated Cloud Function params to Cloud Run
- [GCP only] Remove customized revision naming logic for Cloud Run services to avoid revision name conflicts
- [GCP only] Removed "bazel" terraform module
- [GCP only] Replaced local_file with file function
- [GCP only] Replaced MD5 with SHA-256 hash for cloud functions object names
- [GCP only] Set up foundational infrastructure for HTTP status code migration from 4xx to 5xx
- [GCP only] Skipped uploading zip files if Cloud Run is enabled

## [1.14.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.13.0...v1.14.0) (2024-12-05)

### Important note
- **[GCP only]** Configure the maximum number of concurrent requests processed by each Public Key API Cloud Function instance by setting below Terraform variables
    ```
    get_public_key_cpus = 2
    get_public_key_request_concurrency = 10
    ```
- **[GCP only]** Manually update the Spanner database **only when not using** the `deploy_distributedpbs.sh` script.
    ```
    pbs_database_name=<customize this>
    pbs_database_instance_name=<customize this>
    proto_bundle_file_path=<customize this>
    gcp_project_id=<customize this>
    pbs_spanner_budget_key_table_name=<customize this>
    all_bundles="(\`privacy_sandbox_pbs.BudgetValue\`)"

    gcloud spanner databases ddl update $pbs_database_name \
        --instance=$pbs_database_instance_name \
        --ddl="CREATE PROTO BUNDLE $all_bundles;" \
        --proto-descriptors-file=$proto_bundle_file_path \
        --project=$gcp_project_id

    gcloud spanner databases ddl update $pbs_database_name \
        --instance=$pbs_database_instance_name \
        --ddl="ALTER TABLE $pbs_spanner_budget_key_table_name ADD COLUMN IF NOT EXISTS ValueProto privacy_sandbox_pbs.BudgetValue;" \
        --project=$gcp_project_id
    ```
- **[GCP only]** If rolling back from this version with Cloud Functions enabled for MPKHS, the following commands must be run manually for `mpkhs_secondary`.
    ```
    terraform state mv module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_url_map.key_storage[0] module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_url_map.key_storage
    terraform state mv module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_backend_service.key_storage[0] module.multipartykeyhosting_secondary.module.keystorageservice.google_compute_backend_service.key_storage
    ```

### Changes

- Added `benchmark_deps.bzl` function to load dependencies of `google_benchmark`.
- Documented the PBS metrics in `METRICS.md`.
- Enabled OTel for the PBS local debugging script.
- Introduced a new `JobParameters` proto.
- Optimized the performance of the `x-auth-token` refresh token implementation by refreshing tokens only when expired.
- Refactored logging statements across the codebase to use `absl::StrFormat()` for compile-time format string validation.
- Standardized C++ includes in the codebase to consistently use the `cc/` prefix.
- Updated `FSBlobStorageClient` to include full paths in blob keys for improved accuracy.
- Updated container dependencies.
- Updated the CreateJob request and GetJob response API schemas to accept an optional input prefix list.
- Updated the bucket boundaries of histogram metrics to better capture data distribution.
- Updated the method of specifying PBS requests' `User-Agent` header. The new method involves setting the value via the `@TrustedServicesClientVersion` annotation. Users of the library should bind a value to this annotation.
- [AWS only] Added optional Terraform to send metrics to Google Cloud, using CloudWatch Metric Streams.
- [AWS only] Configured Terraform to ignore changes in `read_capacity` and `write_capacity` because an autoscaling policy is attached to the table.
- [AWS only] Fixed `setup_enclave.sh` script to terminate immediately upon any error.
- [AWS only] Removed partitioned PBS code in `pbs_server`.
- [AWS only] Set the explicit volume size of aggregation worker AMI.
- [GCP Only] Added KMS Key rotation period set to 31536000s (1 year).
- [GCP only] Added Golang module backupfunction, responsible for initiating a Spanner Backup against PBS and MPKHS databases.
- [GCP only] Added a Bazel build target for a KHS-only tarball.
- [GCP only] Added a helper script and Bazel rule for building and uploading the PBS container.
- [GCP only] Added the Terraform `get_public_key_cpus` and `get_public_key_request_concurrency` variables to control the number of CPUs used by and the maximum number of concurrent requests processed by each Public Key API Cloud Function instance.
- [GCP only] Configured the dedicated load balancer resources for Cloud Run MPKHS to allow streamlined switching between Cloud Functions and Cloud Run.
- [GCP only] Disabled module and plugin upgrades during PBS Terraform deployments.
- [GCP only] Introduced a new `BudgetValue` proto. The proto descriptor is included in the coordinator tarball at `dist/budget_value_proto-descriptor-set.proto.bin`. The PBS deployment script now updates the proto bundle and creates a corresponding `ValueProto` column in the PBS Spanner database.
- [GCP only] Removed unused `VERSION` and `LOG_EXECUTION_ID` environment variables from Cloud Run services. Added an `EXPORT_OTEL_METRICS` environment variable and a `version` label to these services.
- [GCP only] Set the explicit region for `the key_generation_cron` Cloud Scheduler job to ensure it uses the same region as the key generation VM.
- [GCP only] Updated the Cloud Run configuration to align container resources, scaling, and timeout settings with those of Cloud Functions.
- [GCP only] Updated the PBS VM update policy to ensure new machines are created and running before old ones are removed.
- [GCP only] Updated the minimum allowed GCP Terraform provider version to `5.37.0` for all modules.

## [1.13.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.12.0...v1.13.0) (2024-11-06)

- **Important note**
    - **[GCP only]** Cloud Run PBS
      - In the `auto.tfvars` for `distributedpbs_application`, add the following variables:
        ```
        pbs_cloud_run_max_instances = 20
        pbs_cloud_run_min_instances = 10
        pbs_cloud_run_traffic_percentage = 0
        deploy_pbs_cloud_run = false
        enable_pbs_cloud_run = false
        ```
        These terraform variables are needed to configure the Cloud Run PBS backend and Load Balancer. `deploy_pbs_cloud_run = true` will
        instantiate the Cloud Run PBS backend but will not link it to the Load balancer. `enable_pbs_cloud_run = true` requires `deploy_pbs_cloud_run = true`
        and will link the Cloud Run PBS backend to the Load balancer.
          - In a separate, second deployment, set `deploy_pbs_cloud_run = true` and `enable_pbs_cloud_run = true` to configure
            Cloud Run PBS to serve traffic

      - Rollback for **distributedpbs_application** requires two separate deployments:
        1. Set `enable_pbs_cloud_run = false`, run `terraform apply`
        2. Set `deploy_pbs_cloud_run = false`, run `terraform apply`

      - Manual Deployment Steps Required
        - Added optional Spanner autoscaling configuration for PBS storage.
          `pbs_spanner_instance_processing_units` no longer defaults to 1000. Either
          set this explicitly to disable autoscaling, or set
          `pbs_spanner_autoscaling_config` to enable it.

### Changes
  - [GCP only] Renamed Cloud Build substitution variable from `_OUTPUT_IMAGE_NAME` to `_OUTPUT_KEYGEN_IMAGE_NAME` for the artifacts build
  - [GCP only] Added support of Cloud Run for MPKHS
  - [GCP only] Created helper script for building MPKHS components
  - [GCP only] Enabled Cloud Run as an option in addition to GCE for running PBS services
  - [GCP only] Improved metric collection for http.server.request.duration
  - Added default values for deploy and enable tfvars for Cloud Run PBS migration
  - Added metric_router for pbs instance
  - Added missing default value for PBS Cloud Run max concurrency config
  - Corrected README.md file for GCP deployment by removing references to AWS
  - Created benchmark test for UUID FromString method
  - Created benchmark tests for AsyncExecutor
  - Enabled gperftools in the unit tests
  - Enabled health checks according to container type
  - Implemented new LogProvider that writes structured JSON logs to stdout
  - Improved metric collection for PBS services
  - Improved UUID FromString by optimizing hex to int method
  - Refactored GCP Instance Client Provider to be platform-agnostic (should be able to run on GCE and Cloud Run)
  - Refactored Load Balancer config to use one variable for traffic splitting between GCE and Cloud Run
  - Removed counter metric http.client.response
  - Removed default values for PBS traffic split config from module level
  - Removed Http server specific otel flag
  - Updated auth function so that only necessary variables are globalized
  - Updated container dependencies
  - Updated HTTP metric labels
  - Updated the pbs request header to specify AgS version as "User-Agent"
  - Upgraded google-cloud-cpp dependency
  - Upgraded Java dependency slf4j to the latest STABLE release

## [1.12.0](https://github.com/privacysandbox/coordinator-services-and-shared-libraries/compare/v1.11.0...v1.12.0) (2024-10-21)

- **Important note**
  - **[GCP only]** Provider definitions have been removed from MPKHS Terraform. Ensure that the default project and region are set appropriately at the root module level

### Changes

  - Added KHS http request duration metric for private key fetching
  - Addressed crash in BudgetKeyTimeframeManager
  - Addressed crash in ListRecentEncryptionKeys when invalid keys present in database
  - Addressed UAF triggered by observable metric callbacks
  - Created Http server metrics labels in the terraform
  - Updated dependencies to address security vulnerabilities
  - [Aggregatable Report Accounting] Addressed dependency cycle in PBSInstanceV3 initialization
  - [Aggregatable Report Accounting] Cleaned up the Terraform flag "google_scp_pbs_adtech_site_as_authorized_domain_enabled"
  - [Aggregatable Report Accounting] Reduced the severity of logs when async context received error status due to false alarms
  - [Aggregatable Report Accounting] Replaced absl::bind_front with std::bind_front
  - [Aggregatable Report Accounting] Updated C++ version to C++20, except for `//cc/aws/proxy/...`
  - [Aggregatable Report Accounting] Updated consume_budget.cc to reduce map lookup operations
  - [Aggregatable Report Accounting] Updated documentation for PBS Cloud Spanner schema
  - [AWS only] Added missing SSM permissions for SSH access for AWS PBS
  - [GCP only] Added missing project in domain_a_records module
  - [GCP only] Added terraform code to enable OTel metrics for public key, private key, and key storage services
          - To enable, add `export_otel_metrics = true` to the corresponding `mpkhs/<env>.auto.tfvars`
  - [GCP only] Removed provider definitions from MPKHS modules

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
