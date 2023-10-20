# InstanceClient

Responsible for fetching cloud instance metadata.

# Build

## Building the client

    bazel build cc/public/cpio/interface/instance_client/...

## Running tests

    bazel test cc/public/cpio/adapters/instance_client/... && bazel test cc/cpio/client_providers/instance_client_provider/...

# Example

This example [here](/cc/public/cpio/examples/local_instance_client_test.cc) could be run locally. This example [here](/cc/public/cpio/examples/instance_client_test.cc) needs to be run on cloud.
