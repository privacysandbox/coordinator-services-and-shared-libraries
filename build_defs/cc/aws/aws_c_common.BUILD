# Description:
#   AWS C Common

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "aws_c_common",
    srcs = glob([
        "include/aws/common/*.h",
        "include/aws/common/private/*.h",
        "source/*.c",
        "source/posix/*.c",
    ]),
    hdrs = [
        "include/aws/common/config.h",
    ],
    defines = [
        "AWS_AFFINITY_METHOD",
    ],
    includes = [
        "include",
    ],
    textual_hdrs = glob([
        "include/**/*.inl",
    ]),
)

genrule(
    name = "config_h",
    srcs = [
        "include/aws/common/config.h.in",
    ],
    outs = [
        "include/aws/common/config.h",
    ],
    cmd = "sed 's/cmakedefine/undef/g' $< > $@",
)
