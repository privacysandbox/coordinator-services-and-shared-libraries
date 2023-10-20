# CryptoClient

Responsible to encrypt and decrypt data. It supports both one-shot and bi-directional HPKE encryptions.

For HPKE encryption, refer to [RFC9180](https://www.rfc-editor.org/rfc/rfc9180.html) for more information, and refer to [REF9180, Section 9.8](https://www.rfc-editor.org/rfc/rfc9180.html#section-9.8) for bi-directional encryption.

# Build

## Building the client

    bazel build cc/public/cpio/interface/crypto_client/...

## Running tests

    bazel test cc/public/cpio/adapters/crypto_client/... && bazel test cc/cpio/client_providers/crypto_client_provider/...

# Example

See the example [here](/cc/public/cpio/examples/crypto_client_test.cc).
