# CPIO

Provides a unified interface to talk to different cloud platforms.
AWS and GCP are supported currently.

# Build

## Prerequisites

This project needs to be built using [Bazel](https://bazel.build/install).
To get reproducible build, this project needs to be built inside container.

## Building the project
A flag is defined to choose cloud platform at building time:
    `//cc/public/cpio/interface:platform`. Supported values are "aws" and "gcp" currently.
A flag is defined to choose running the code inside TEE or not: `//cc/public/cpio/interface:run_inside_tee`. Supported values are "True" and "False".

Choose to import AWS SDK source code dependencies by running:

        ln -rsf build_defs/cc/sdk_source_code.bzl build_defs/cc/sdk.bzl

An example build command is as follows:

        bazel build --//cc/public/cpio/interface:platform=aws --//cc/public/cpio/interface:run_inside_tee=True cc/public/cpio/...

## Running tests

        bazel test cc/public/cpio/... && bazel test cc/cpio/...

# Using CPIO
Before using any CPIO clients, CPIO needs to be initialized by calling Cpio::InitCpio. After all the usage, CPIO needs to be cleaned up by calling Cpio::ShutdownCpio. In between, any clients can be created and used following this pattern:

        client = ClientFactory::Create(options);
        client->Init();
        client->Run();
        # Use the client
        ...
        client->Stop();

# Layout

1. [cc/public/cpio/interface](interface) and [cc/public/core/interface](/cc/public/core/interface): interfaces and all other public visible targets provided to users of this project.
2. [cc/public/cpio/mock](mock): public visible targets to help with unit tests.
3. [cc/public/cpio/test](test): public visible targets to help with integration tests.
4. [cc/public/cpio/examples](examples): example codes for different clients.
5. [cc/public/cpio/examples/deploy](examples/deploy): example script to deploy binary to Nitro Enclave.
6. [cc/public/cpio/adapters](adapters), [cc/public/cpio/core](core/), [cc/cpio](/cc/cpio) and [cc/core](/cc/core): implementations. The targets there are not public visible.
7. [build_defs/cc](/build_defs/cc), [WORKSPACE](/WORKSPACE): external dependencies.
8. [cc/public/cpio/build_deps](build_deps): scripts to help build reproducible targets inside container.
9. [cc/public/cpio/utils](utils): abstract some common functionalities above clients to help the customers to use the clients more conveniently.

# Clients
1. [BlobStorageClient](interface/blob_storage_client)
2. [CryptoClient](interface/crypto_client)
3. [InstanceClient](interface/instance_client)
4. [KmsClient](interface/kms_client)
5. [MetricClient](interface/metric_client)
6. [ParameterClient](interface/parameter_client)
7. [PrivateKeyClient](interface/private_key_client)
8. [PublicKeyClient](interface/public_key_client)

# Client helpers
There are several utility helpers above Clients to help the customers to use the clients more conveniently. Currently we have:
1. [ConfigurationFetcher](utils/configuration_fetcher): help to fetch configurations through ParameterClient.
2. [MetricAggregation](utils/metric_aggregation): help to pre-aggregate metrics for high-qps traffic.
3. [SyncUtils](utils/sync_utils): to help converting async calls to sync calls.
