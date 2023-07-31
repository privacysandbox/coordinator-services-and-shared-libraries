load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "licenses",
    srcs = glob(
        ["*"],
        exclude = [
            "BUILD",
            "deps_and_licenses.csv",
            "licenses.xml",
            "README.md",
        ],
    ),
    prefix = "licenses",
)

pkg_tar(
    name = "licenses_tar",
    srcs = [
        ":licenses",
    ],
)
