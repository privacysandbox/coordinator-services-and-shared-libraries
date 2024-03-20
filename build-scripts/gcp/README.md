# Building coordinator services artifacts for Google Cloud

## Prerequisites

### Set up GCloud CLI

Make sure you [install](https://cloud.google.com/sdk/gcloud) and
[authenticate](https://cloud.google.com/sdk/docs/authorizing#auth-login) the latest gcloud CLI.

### Create a Docker image repo under Google Cloud Artifact Registry

If you haven't done so, create a Docker image repo under Google Cloud Artifact Registry. Instructions
can be found [here](https://cloud.google.com/artifact-registry/docs/repositories/create-repos).
This will be used for uploading the Docker image for the build container and the keygen image.

### Create a GCS bucket for uploading output artifacts

If you haven't done so, create a GCS bucket for uploading the output artifacts, following the
[instructions](https://cloud.google.com/storage/docs/creating-buckets#storage-create-bucket-console).
This will be used for uploading the terraform tar file for deployment.

## Building the build container

The build container is requried for the coordinator services artifacts build.

To trigger the build, run the following command from the root of the repo:

```sh
gcloud builds submit --config=build-scripts/gcp/build-container/cloudbuild.yaml --substitutions=_BUILD_IMAGE_REPO_PATH="<YourBuildContainerRegistryRepoPath>",_BUILD_IMAGE_NAME="bazel-build-container",_BUILD_IMAGE_TAG="<YourBuildContainerImageTag>"
```

The `<YourBuildContainerRegistryRepoPath>` is in the form of
`<location>-docker.pkg.dev/<project-id>/<artifact-repository-name>`.

The build can take several minutes. You can check the status at
`https://console.cloud.google.com/cloud-build/builds`.

## Building artifacts

To build the coordinator services artifacts the above build container is required. Make sure the
build for the build container ran successful before starting the build for the coordinator services
artifacts.

To trigger the artifacts build, run the following command from the root of the repo:

```sh
gcloud builds submit --config=build-scripts/gcp/cloudbuild.yaml --substitutions=_BUILD_IMAGE_REPO_PATH="<YourBuildContainerRegistryRepoPath>",_BUILD_IMAGE_NAME="bazel-build-container",_BUILD_IMAGE_TAG="<YourBuildContainerImageTag>",_OUTPUT_IMAGE_REPO_PATH="<YourOutputContainerRegistryRepoPath>",_OUTPUT_IMAGE_NAME="keygen_mp_gcp_prod",_OUTPUT_IMAGE_TAG="<YourOutputImageCustomTag>",_TAR_PUBLISH_BUCKET="<YourArtifactsOutputBucketName>",_TAR_PUBLISH_BUCKET_PATH="<YourArtifactsOutputRelativePath>"
```

The build can take several minutes. You can check the status at
`https://console.cloud.google.com/cloud-build/builds`.

## Download artifacts

To download the artifacts you can use the `gsutil` command. Download the artifacts to
`<repository_root>/terraform/gcp/tar`. Run the following in `<repository_root>/terraform/gcp`

```sh
mkdir -p tar
gsutil cp -r gs://<YourArtifactsOutputBucketName>/<YourArtifactsOutputRelativePath>/$(cat ../../version.txt)/ tar/
tar -xzf "ps-gcp-multiparty-coordinator-$(cat ../../version.txt).tgz" -C <repository_root>/terraform/gcp
```
