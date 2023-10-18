load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "jobservice",
    srcs = glob(
        ["*"],
        exclude = ["BUILD"],
    ),
    prefix = "jobservice",
)

alias(
    name = "frontendCloudFunctionDeploy",
    actual = "//java/com/google/scp/operator/frontend/service/gcp:FrontendServiceHttpCloudFunctionDeploy",
)

alias(
    name = "workerScaleInCloudFunctionDeploy",
    actual = "//java/com/google/scp/operator/autoscaling/app/gcp:WorkerScaleInCloudFunctionDeploy",
)
