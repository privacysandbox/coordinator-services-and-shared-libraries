# ConfigClient

Responsible for fetching pre-stored application data or cloud metadata.  
In AWS, the application data could be pre-stored in ParameterStore and surfaced
through GetParameter function. EC2 istances can also be used to pre-create tags which are
surfaced through GetTag function.

# Build

## Building the client

    bazel build cc/public/cpio/interface/config_client/...

## Running tests

    bazel test cc/public/cpio/adapters/config_client/... && bazel test cc/cpio/client_providers/config_client_provider/...

# Example

This example [here](/cc/public/cpio/examples/local_config_client_test.cc) could be run locally. This example [here](/cc/public/cpio/examples/config_client_test.cc) needs to be run inside EC2 instance.

