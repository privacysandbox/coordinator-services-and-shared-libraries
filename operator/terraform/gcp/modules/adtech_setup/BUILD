load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "adtech_setup",
    srcs = glob(
        ["*"],
        exclude = ["BUILD"],
    ),
    prefix = "adtech_setup",
)
