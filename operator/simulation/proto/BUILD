load("@rules_java//java:defs.bzl", "java_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "aggregate_service_key_proto",
    srcs = ["aggregate_service_key.proto"],
)

java_proto_library(
    name = "aggregate_service_key_java_proto",
    deps = [":aggregate_service_key_proto"],
)

proto_library(
    name = "conversion_report_proto",
    srcs = ["conversion_report.proto"],
)

java_proto_library(
    name = "conversion_report_java_proto",
    deps = [":conversion_report_proto"],
)
