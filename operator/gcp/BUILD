load("//build_defs:release.bzl", "gcs_package_release")

package(default_visibility = ["//visibility:public"])

gcs_package_release(
    name = "operator_gcp_release",
    artifact_base_name = "scp-operator-gcp-{VERSION}.tgz",
    package_target = "//operator/terraform/gcp:operator_tar",
    release_bucket = "admcloud-scp-repo-dev",
    release_key = "operator-gcp",
)
