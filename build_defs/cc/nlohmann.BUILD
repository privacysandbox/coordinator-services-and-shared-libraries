package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "lib",
    srcs = glob(
        [
            "single_include/nlohmann/json.hpp",
            "include/nlohmann/*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    includes = ["single_include"],
)
