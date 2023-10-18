# ParameterClient

Responsible for manage parameter metadata in cloud.
In AWS, the parameter metadata are stored in ParameterStore.
In GCP, the parameter metadata are stored in SecretManager.

# Build

## Building the client

    bazel build cc/public/cpio/interface/parameter_client/...

## Running tests

    bazel test cc/public/cpio/adapters/parameter_client/... && bazel test cc/cpio/client_providers/parameter_client_provider/...

# Example

This example [here](/cc/public/cpio/examples/local_parameter_client_test.cc) could be run locally. This example [here](/cc/public/cpio/examples/parameter_client_test.cc) needs to be run on cloud.
