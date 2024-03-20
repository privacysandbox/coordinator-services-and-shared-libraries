load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "distributedpbs_application_shared",
    srcs = glob(
        ["*"],
        exclude = ["BUILD"],
    ),
    prefix = "distributedpbs_application",
)
