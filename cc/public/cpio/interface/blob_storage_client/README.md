# BlobStorageClient

Responsible for interacting with the BlobStorage service (Get/Put/List/Delete).
In AWS, this is S3. In GCP, this is Google Cloud Storage.

# Build

## Building the client

    bazel build cc/public/cpio/interface/blob_storage_client/...

## Running tests

    bazel test cc/public/cpio/adapters/blob_storage_client/... && bazel test cc/cpio/client_providers/blob_storage_client_provider/...

# Example

See the example [here](/cc/public/cpio/examples/blob_storage_client_test.cc).
