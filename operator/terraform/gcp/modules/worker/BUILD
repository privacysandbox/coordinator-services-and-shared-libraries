load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "worker",
    srcs = glob(
        ["*"],
        exclude = [
            "BUILD",
        ],
    ),
    prefix = "worker",
)
