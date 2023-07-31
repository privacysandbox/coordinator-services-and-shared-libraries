package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "v8",
    srcs = ["libv8_monolith.a"],
    hdrs = glob(["include/**/*.h"]),
    copts = ["-std=c++17"],
    defines = ["V8_COMPRESS_POINTERS"],
    includes = ["include"],
    linkopts = [
        "-pthread",
        "-ldl",
    ],
)
