# Building Coordinator Services

## Optional: Build and publish CC build container

The container image used to build the coordinator services is generated with the bazel rule `container_to_build_cc` within `//cc/tools/build`. A prebuilt image is available and configured.
To use a self-built build container follow the steps below.

### Create ECR repository

Install AWS CLI and Docker follwing [instructions](https://docs.aws.amazon.com/AmazonECR/latest/userguide/getting-started-cli.html)

Create private repository:

```sh
export AWS_REPOSITORY_REGION="us-east-1"
export REPOSITORY_NAME=$(grep -o 'CC_BUILD_CONTAINER_REPOSITORY = "[^"]*"' cc/tools/build/build_container_params.bzl.sample | awk -F'"' '{print $2}')
aws ecr create-repository \
    --repository-name $REPOSITORY_NAME \
    --image-scanning-configuration scanOnPush=true \
    --region $AWS_REPOSITORY_REGION
```

### Build the container

At the root of the repository run

```sh
bazel build //cc/tools/build:container_to_build_cc
```

### Load into Docker, tag and publish to ECR

At the root of the repo run the following commands (replace values in <...>):

```sh
docker load < bazel-bin/cc/tools/build/container_to_build_cc_commit.tar
```

Use container version from `tools/build/build_container_params.bzl`:

```sh
export CONTAINER_VERSION=$(grep -o 'CC_BUILD_CONTAINER_TAG = "[^"]*"' cc/tools/build/build_container_params.bzl.sample | awk -F'"' '{print $2}')
```

Tag the container with the repository and container version (replace <AWS_ACCOUNT_ID> with your account id):

```sh
docker tag bazel/cc/tools/build:container_to_build_cc <AWS_ACCOUNT_ID>.dkr.ecr.$AWS_REPOSITORY_REGION.amazonaws.com/$REPOSITORY_NAME:$CONTAINER_VERSION
```

Upload to ECR (replace <AWS_ACCOUNT_ID> with your account id):

```sh
aws ecr get-login-password \
  --region $AWS_REPOSITORY_REGION | docker login \
  --username AWS \
  --password-stdin <AWS_ACCOUNT_ID>.dkr.ecr.$AWS_REPOSITORY_REGION.amazonaws.com

docker push <AWS_ACCOUNT_ID>.dkr.ecr.$AWS_REPOSITORY_REGION.amazonaws.com/$REPOSITORY_NAME:$CONTAINER_VERSION
```

### Use the container in coordinator artifacts build

After the above steps, copy the file "cc/tools/build/build_container_params.bzl.sample" to "cc/tools/build/build_container_params.bzl"

```sh
cp cc/tools/build/build_container_params.bzl.sample cc/tools/build/build_container_params.bzl
```

and replace the CC_BUILD_CONTAINER_REGISTRY value with  `<AWS_ACCOUNT_ID>.dkr.ecr.$AWS_REPOSITORY_REGION.amazonaws.com` (replace <AWS_ACCOUNT_ID> with your account id).

## Building coordinator artifacts

### Using prebuilt CC build container

If you skipped the above steps to build a self-built build container and want to use our prebuilt one, run the following command:

```sh
cp cc/tools/build/build_container_params.bzl.prebuilt cc/tools/build/build_container_params.bzl
```

### Build key-generation AMI

Set required environment variables:

```sh
export COORDINATOR_VERSION=$(cat version.txt)
export KEY_GENERATION_LOG=$(pwd)/buildlog.txt
export KEY_GENERATION_AMI="aggregation-service-keygeneration-enclave-prod-${COORDINATOR_VERSION}"
export AWS_BUILD_REGION="us-east-1"
```

Run keygeneration AMI build and extract PCR0:

```sh
bazel run //coordinator/keygeneration/aws:prod_keygen_ami \
  --//coordinator/keygeneration/aws:keygeneration_ami_name="${KEY_GENERATION_AMI}" \
  --//coordinator/keygeneration/aws:keygeneration_ami_region="${AWS_BUILD_REGION}" | tee "${KEY_GENERATION_LOG}"

grep -o '"PCR0": "[^"]*"' "${KEY_GENERATION_LOG}" | awk -F'"' '{print $4}'
```

### Build coordinator deployment archive

Set required environment variables (replace <AWS_ACCOUNT_ID> with your account id):

```sh
export COORDINATOR_VERSION=$(cat version.txt)
export COORDINATOR_TAR_FILE=$(bazel info bazel-bin)/coordinator/terraform/aws/multiparty_coordinator_tar.tgz
export KEY_GENERATION_AMI="aggregation-service-keygeneration-enclave-prod-${COORDINATOR_VERSION}"
export AWS_ACCOUNT_ID=<AWS_ACCOUNT_ID>
```

Build the deployment archive:

```sh
bazel build //coordinator/terraform/aws:multiparty_coordinator_tar
```

Add keygeneration AMI configuration to built archive:

```sh
export TAR_MANIPULATION_TMP_DIR=$(mktemp -d -t ci-XXXXXXXXXX)
gunzip < $COORDINATOR_TAR_FILE > $TAR_MANIPULATION_TMP_DIR/scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar
pushd $TAR_MANIPULATION_TMP_DIR
mkdir -p environments_mp_primary/shared/mpkhs_primary/ && mkdir -p environments_mp_primary/demo/mpkhs_primary/

cat <<EOT >> environments_mp_primary/shared/mpkhs_primary/ami_params.auto.tfvars
##########################################################
# Prefiled values for AMI lookup based on relase version #
##########################################################

key_generation_ami_name_prefix = "${KEY_GENERATION_AMI}"
key_generation_ami_owners      = ["${AWS_ACCOUNT_ID}"]
EOT

ln -s ../../shared/mpkhs_primary/ami_params.auto.tfvars environments_mp_primary/demo/mpkhs_primary/ami_params.auto.tfvars
tar --append --file=scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar environments_mp_primary/shared/mpkhs_primary/ami_params.auto.tfvars environments_mp_primary/demo/mpkhs_primary/ami_params.auto.tfvars

tar --list --file=scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar

popd
gzip < $TAR_MANIPULATION_TMP_DIR/scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar > multiparty-coordinator-${COORDINATOR_VERSION}.tgz

rm -rf $TAR_MANIPULATION_TMP_DIR
unset TAR_MANIPULATION_TMP_DIR
```

You find the built archive in your repository root with the name `multiparty-coordinator-${COORDINATOR_VERSION}.tgz`
