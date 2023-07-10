# Description:
#   AWS C Event Stream

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "aws_c_event_stream",
    srcs = glob([
        "include/**/*.h",
        "source/**/*.c",
    ]),
    includes = [
        "include",
    ],
    deps = [
        "@aws_c_common",
        "@aws_checksums",
    ],
)
