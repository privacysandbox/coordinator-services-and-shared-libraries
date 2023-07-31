# PrivateKeyClient

Responsible for fetching private keys which could be used to decrypt payload. The key IDs and corresponding public keys could be fetched through the [PublicKeyClient](/cc/public/cpio/interface/public_key_client/README.md).

# Build

## Building the client

    bazel build cc/public/cpio/interface/private_key_client/...

## Running tests

    bazel test cc/public/cpio/adapters/private_key_client/... && bazel test cc/cpio/client_providers/private_key_client_provider/...

# Example

This example [here](/cc/public/cpio/examples/local_private_key_client_test.cc) could be run locally. This example [here](/cc/public/cpio/examples/private_key_client_test.cc) needs to be run inside enclave.
