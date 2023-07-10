load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "nghttp2",
    cache_entries = {
        "Boost_THREAD_LIBRARY": "$EXT_BUILD_DEPS/../../boost/libthread.a",
        "Boost_SYSTEM_LIBRARY": "$EXT_BUILD_DEPS/../../boost/libsystem.a",
        "ENABLE_ASIO_LIB": "on",
        "ENABLE_STATIC_LIB": "on",
        "ENABLE_EXAMPLES": "off",
    },
    defines = ["OPENSSL_IS_BORINGSSL=1"],
    env = {"MAKEFLAGS": "-j16"},
    lib_source = ":all_srcs",
    out_shared_libs = [
        "libnghttp2.so.14",
        "libnghttp2_asio.so.1",
    ],
    out_static_libs = [
        "libnghttp2.a",
    ],
    deps = [
        "@boost//:asio_ssl",
        "@boost//:system",
        "@boost//:thread",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
    ],
)
