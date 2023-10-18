# Rule to build an SDK container image and push to GCP
## Usage
In your own BUILD file, define a target simlar like:
```
load("//cc/public/cpio/build_deps/native_lib/gcp:gcp_sdk_lib_container.bzl", "gcp_sdk_lib_container")

gcp_sdk_lib_container(
    name = "gcp_sdk_lib_test",
    client_binaries = {"//path/to/test/target:test_binary":[]},
    image_repository = "existing_test_repository",
    image_registry = "existing_test_registry",
    image_tag = "gcp_sdk_lib_test_image",
    ...
)
```
The input parameters of the function are:

`name`(required): Name used for container push target

`inside_tee`(optional): whether this container is running inside tee. Default to True.

`client_binaries`(required): Dict of bazel targets of client binaries with arguments.
This is in the form:
```
{
    "client_binary_target" : ["arg1", "arg2"]
}
```

`image_repository`(required): An existing repository in GCP we upload the image to.

`image_registry`(required): An existing registry in GCP we upload the image to.

`image_tag`(required): Customer specified image tag.

`additional_files`(optional): Additional files to include in the container root.

`additional_tars`(optional): Additional files include in the container based on their
paths within the tar.

`pkgs_to_install`(optional): Packages to be installed in the container.

`sdk_cmd_override`(optional): "cmd" parameter to use with the container_image.
Only used for testing purposes.

## Build
Use the following command to build the container and AMI:

```
bazel run path/to/where/the/rule/is/used:{name}
```

## Output
1. In GCP, an image with the given image_tag will appear in the given image_repository and image registry.
2. The image hash which can be used for attestation can be found at the end of the "bazel run" log.
