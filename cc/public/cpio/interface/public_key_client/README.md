# PublicKeyClient

Responsible for fetching public keys which could be used to encrypt payload.

# Build

## Building the client

    bazel build cc/public/cpio/interface/public_key_client/...

## Running tests

    bazel test cc/public/cpio/adapters/public_key_client/... && bazel test cc/cpio/client_providers/public_key_client_provider/...

# Example

See the example [here](/cc/public/cpio/examples/public_key_client_test.cc).
