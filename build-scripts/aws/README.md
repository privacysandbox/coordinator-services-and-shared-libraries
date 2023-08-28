# Building coordinator services artifacts

## Prerequisites

### Set up AWS client

Make sure you
[install](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html) and
[set up](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html) the latest
AWS client.

### Set up Terraform

Change into the `<repository_root>/build-scripts/aws/terraform` folder.

The setup scripts require terraform version `1.2.3`. You can download Terraform version `1.2.3` from
[https://releases.hashicorp.com/terraform/1.2.3/](https://releases.hashicorp.com/terraform/1.2.3/)
or _at your own risk_, you can install and use
[Terraform version manager](https://github.com/tfutils/tfenv) instead.

If you have the Terraform version manager `tfenv` installed, run the following in your
`<repository_root>/build-scripts/aws/terraform` to set Terraform to version `1.2.3`.

```sh
tfenv install 1.2.3;
tfenv use 1.2.3
```

We recommend you store the [Terraform state](https://www.terraform.io/language/state) in a cloud
bucket. Create a S3 bucket via the console/cli, which we'll reference as `tf_state_bucket_name`.
Please consider enabling versioning for this bucket.

## Configure CodeBuild Setup

Copy `main.tf_sample` and `codebuild.auto.tfvars_sample` and adjust values. Run in
`<repository_root>/build-scripts/aws/terraform`

```sh
cp main.tf_sample main.tf
cp codebuild.auto.tfvars_sample codebuild.auto.tfvars
```

Open `main.tf` to configure your terraform state backend storage. Open `codebuild.auto.tfvars` to
set build region, artifact_output location and github access credentials. All available variables to
configure can be found in codebuild_variables.tf

Run `terrform init` to setup terraform.

To apply changes run `terraform apply` and follow the prompt

```terraform
Plan: 9 to add, 0 to change, 0 to destroy.
...
```

## Building the build container

The build container is required for the coordinator services artifacts build.

To trigger the build run:

```sh
aws codebuild start-build --project-name bazel-build-container --region <your_aws_region>
```

The build can take several minutes. You can check the status at
`https://<region>.console.aws.amazon.com/codesuite/codebuild/projects`.

## Building artifacts

To build the coordinator services artifacts the above build container is required. Make sure the
build for the build container ran successful before starting the build for the coordinator services
artifacts.

To trigger the build run:

```sh
aws codebuild start-build --project-name coordinator-services-artifacts-build --region <your_aws_region>
```

The build can take several minutes. You can check the status at
`https://<region>.console.aws.amazon.com/codesuite/codebuild/projects`.

## Download artifacts

To download the deployment archive you can use `aws s3` commands.

```sh
aws s3 cp s3://<build_artifacts_output_bucket>/coordinator-services-build-artifacts/$(cat ../../version.txt)/ . --recursive
```
