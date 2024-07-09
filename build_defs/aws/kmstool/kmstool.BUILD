load("@bazel_skylib//rules:copy_directory.bzl", "copy_directory")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(default_visibility = ["//visibility:public"])

exports_files(glob(["*"]))

copy_directory(
    name = "bin_dir",
    src = "bin",
    out = "bin",
)

copy_directory(
    name = "cmake_dir",
    src = "cmake",
    out = "cmake",
)

copy_directory(
    name = "containers_dir",
    src = "containers",
    out = "containers",
)

copy_directory(
    name = "docs_dir",
    src = "docs",
    out = "docs",
)

copy_directory(
    name = "include_dir",
    src = "include",
    out = "include",
)

copy_directory(
    name = "source_dir",
    src = "source",
    out = "source",
)

copy_directory(
    name = "tests_dir",
    src = "tests",
    out = "tests",
)

pkg_tar(
    name = "source_code_tar",
    srcs = [
        ":bin_dir",
        ":cmake_dir",
        ":containers_dir",
        ":docs_dir",
        ":include_dir",
        ":source_dir",
        ":tests_dir",
    ] + glob(["*"]),
    mode = "0777",
    package_dir = "aws_nitro_enclaves_sdk_c",
)
