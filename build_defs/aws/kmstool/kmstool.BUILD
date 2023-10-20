load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(default_visibility = ["//visibility:public"])

exports_files(glob(["*"]))

pkg_tar(
    name = "source_code_tar",
    srcs = [
        "bin",
        "cmake",
        "containers",
        "docs",
        "include",
        "source",
        "tests",
    ] + glob(["*"]),
    mode = "0777",
    package_dir = "aws_nitro_enclaves_sdk_c",
)
