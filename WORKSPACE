load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")

################################################################################
# Rules JVM External: Begin
################################################################################
RULES_JVM_EXTERNAL_TAG = "4.0"

RULES_JVM_EXTERNAL_SHA = "31701ad93dbfe544d597dbe62c9a1fdd76d81d8a9150c2bf1ecf928ecdf97169"

http_archive(
    name = "rules_jvm_external",
    sha256 = RULES_JVM_EXTERNAL_SHA,
    strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/%s.zip" % RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()
################################################################################
# Rules JVM External: End
################################################################################
################################################################################
# Download all http_archives and git_repositories: Begin
################################################################################

# Declare explicit protobuf version, to override any implicit dependencies.
PROTOBUF_CORE_VERSION = "3.19.4"

http_archive(
    name = "com_google_protobuf",
    sha256 = "3bd7828aa5af4b13b99c191e8b1e884ebfa9ad371b0ce264605d347f135d2568",
    strip_prefix = "protobuf-%s" % PROTOBUF_CORE_VERSION,
    urls = [
        "https://github.com/protocolbuffers/protobuf/archive/v%s.tar.gz" % PROTOBUF_CORE_VERSION,
    ],
)

http_archive(
    name = "rules_java",
    sha256 = "34b41ec683e67253043ab1a3d1e8b7c61e4e8edefbcad485381328c934d072fe",
    url = "https://github.com/bazelbuild/rules_java/releases/download/4.0.0/rules_java-4.0.0.tar.gz",
)

# Load specific version of differential privacy from github.

DIFFERENTIAL_PRIVACY_COMMIT = "68bdbb24fe493638d937120c08927398604c55af"

# value recommended by the differential privacy repo.
# date, not after the specified commit to allow for more shallow clone of repo
# for faster build times.
DIFFERENTIAL_PRIVACY_SHALLOW_SINCE = "1618997113 +0200"

git_repository(
    name = "com_google_differential_privacy",
    commit = DIFFERENTIAL_PRIVACY_COMMIT,
    remote = "https://github.com/google/differential-privacy.git",
    shallow_since = DIFFERENTIAL_PRIVACY_SHALLOW_SINCE,
)

#############
# PKG Rules #
#############

http_archive(
    name = "rules_pkg",
    sha256 = "a89e203d3cf264e564fcb96b6e06dd70bc0557356eb48400ce4b5d97c2c3720d",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.5.1/rules_pkg-0.5.1.tar.gz",
        "https://github.com/bazelbuild/rules_pkg/releases/download/0.5.1/rules_pkg-0.5.1.tar.gz",
    ],
)

############
# Go rules #
############

# Note: Go build rules are an indirect dependency of "io_bazel_rules_docker" and
# a direct dependency of rpmpack. These rules are not used for deploying go code
# at the time of writing.

http_archive(
    name = "io_bazel_rules_go",
    sha256 = "f2dcd210c7095febe54b804bb1cd3a58fe8435a909db2ec04e31542631cf715c",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.31.0/rules_go-v0.31.0.zip",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.31.0/rules_go-v0.31.0.zip",
    ],
)

http_archive(
    name = "bazel_gazelle",
    sha256 = "de69a09dc70417580aabf20a28619bb3ef60d038470c7cf8442fafcf627c21cb",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.24.0/bazel-gazelle-v0.24.0.tar.gz",
        "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.24.0/bazel-gazelle-v0.24.0.tar.gz",
    ],
)

###################
# Container rules #
###################

# Note: these rules add a dependency on the golang toolchain and must be ordered
# after any `go_register_toolchains` calls in this file (or else the toolchain
# defined in io_bazel_rules_docker are used for future go toolchains)
http_archive(
    name = "io_bazel_rules_docker",
    sha256 = "59d5b42ac315e7eadffa944e86e90c2990110a1c8075f1cd145f487e999d22b3",
    strip_prefix = "rules_docker-0.17.0",
    urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v0.17.0/rules_docker-v0.17.0.tar.gz"],
)

load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)

#############
# CPP Rules #
#############

http_archive(
    name = "com_google_googletest",
    sha256 = "8daa1a71395892f7c1ec5f7cb5b099a02e606be720d62f1a6a98f8f8898ec826",
    strip_prefix = "googletest-e2239ee6043f73722e7aa812a459f54a28552929",
    urls = ["https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip"],
)

http_archive(
    name = "rules_cc",
    sha256 = "b295cad8c5899e371dde175079c0a2cdc0151f5127acc92366a8c986beb95c76",
    strip_prefix = "rules_cc-daf6ace7cfeacd6a83e9ff2ed659f416537b6c74",
    urls = ["https://github.com/bazelbuild/rules_cc/archive/daf6ace7cfeacd6a83e9ff2ed659f416537b6c74.zip"],
)

################
# Python Rules #
################

http_archive(
    name = "rules_python",
    sha256 = "a30abdfc7126d497a7698c29c46ea9901c6392d6ed315171a6df5ce433aa4502",
    strip_prefix = "rules_python-0.6.0",
    url = "https://github.com/bazelbuild/rules_python/archive/0.6.0.tar.gz",
)

###############
# Proto rules #
###############

http_archive(
    name = "rules_proto",
    sha256 = "66bfdf8782796239d3875d37e7de19b1d94301e8972b3cbd2446b332429b4df1",
    strip_prefix = "rules_proto-4.0.0",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/refs/tags/4.0.0.tar.gz",
        "https://github.com/bazelbuild/rules_proto/archive/refs/tags/4.0.0.tar.gz",
    ],
)

# Download the AWS enclave SDK repo and apply a patch for building the kmstool dependencies.
load("//build_defs/shared:enclaves_kmstools.bzl", "import_aws_nitro_enclaves_sdk_c")

import_aws_nitro_enclaves_sdk_c()

###########################
# Binary Dev Dependencies #
###########################

http_archive(
    name = "packer",
    build_file_content = """
package(default_visibility = ["//visibility:public"])
exports_files(["packer"])
""",
    sha256 = "57d0411e578aea62918d36ed186951139d5d49d44b76e5666d1fbf2427b385ae",
    url = "https://releases.hashicorp.com/packer/1.8.6/packer_1.8.6_linux_amd64.zip",
)

http_archive(
    name = "terraform",
    build_file_content = """
package(default_visibility = ["//visibility:public"])
exports_files(["terraform"])
""",
    sha256 = "728b6fbcb288ad1b7b6590585410a98d3b7e05efe4601ef776c37e15e9a83a96",
    url = "https://releases.hashicorp.com/terraform/1.2.3/terraform_1.2.3_linux_amd64.zip",
)

# google cloud sdk for releasing artifacts to gcs
http_archive(
    name = "google-cloud-sdk",
    build_file_content = """
package(default_visibility = ["//visibility:public"])
exports_files(["google-cloud-sdk"])
""",
    # latest from https://cloud.google.com/storage/docs/gsutil_install#linux as of 2021-12-16
    sha256 = "94328b9c6559a1b7ec2eeaab9ef0e4702215e16e8327c5b99718750526ae1efe",
    url = "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-sdk-367.0.0-linux-x86_64.tar.gz",
)

# google-java-format for presubmit checks of format and unused imports
http_file(
    name = "google_java_format",
    downloaded_file_path = "google-java-format.jar",
    sha256 = "a356bb0236b29c57a3ab678f17a7b027aad603b0960c183a18f1fe322e4f38ea",
    urls = ["https://github.com/google/google-java-format/releases/download/v1.15.0/google-java-format-1.15.0-all-deps.jar"],
)

git_repository(
    name = "com_github_google_rpmpack",
    # Lastest commit in main branch as of 2021-11-29
    commit = "d0ed9b1b61b95992d3c4e83df3e997f3538a7b6c",
    remote = "https://github.com/google/rpmpack.git",
    shallow_since = "1637822718 +0200",
)

# Note: requires golang toolchain.
http_archive(
    name = "com_github_bazelbuild_buildtools",
    sha256 = "ae34c344514e08c23e90da0e2d6cb700fcd28e80c02e23e4d5715dddcb42f7b3",
    strip_prefix = "buildtools-4.2.2",
    urls = [
        "https://github.com/bazelbuild/buildtools/archive/refs/tags/4.2.2.tar.gz",
    ],
)

###################
# CC Dependencies #
###################

new_git_repository(
    name = "moodycamel_concurrent_queue",
    build_file = "//build_defs/cc:moodycamel.BUILD",
    # Commited Mar 20, 2022
    commit = "22c78daf65d2c8cce9399a29171676054aa98807",
    remote = "https://github.com/cameron314/concurrentqueue.git",
    shallow_since = "1647803790 -0400",
)

new_git_repository(
    name = "nlohmann_json",
    build_file = "//build_defs/cc:nlohmann.BUILD",
    # Commits on Apr 6, 2022
    commit = "15fa6a342af7b51cb51a22599026e01f1d81957b",
    remote = "https://github.com/nlohmann/json.git",
)

git_repository(
    name = "oneTBB",
    # Commits on Apr 18, 2022
    commit = "9d2a3477ce276d437bf34b1582781e5b11f9b37a",
    remote = "https://github.com/oneapi-src/oneTBB.git",
    shallow_since = "1648820995 +0300",
)

load("//build_defs/cc/aws:aws_sdk_cpp_deps.bzl", "import_aws_sdk_cpp")

import_aws_sdk_cpp()

# Boost
# latest as of 2022-06-09
_RULES_BOOST_COMMIT = "789a047e61c0292c3b989514f5ca18a9945b0029"

http_archive(
    name = "com_github_nelhage_rules_boost",
    sha256 = "c1298755d1e5f458a45c410c56fb7a8d2e44586413ef6e2d48dd83cc2eaf6a98",
    strip_prefix = "rules_boost-%s" % _RULES_BOOST_COMMIT,
    urls = [
        "https://github.com/nelhage/rules_boost/archive/%s.tar.gz" % _RULES_BOOST_COMMIT,
    ],
)

http_archive(
    name = "rules_foreign_cc",
    sha256 = "6041f1374ff32ba711564374ad8e007aef77f71561a7ce784123b9b4b88614fc",
    strip_prefix = "rules_foreign_cc-0.8.0",
    url = "https://github.com/bazelbuild/rules_foreign_cc/archive/0.8.0.tar.gz",
)

# nghttp2
http_archive(
    name = "com_github_nghttp2_nghttp2",
    build_file = "//build_defs/cc:nghttp2.BUILD",
    patch_args = ["-p1"],
    patches = ["//build_defs/cc:nghttp2.patch"],
    sha256 = "62f50f0e9fc479e48b34e1526df8dd2e94136de4c426b7680048181606832b7c",
    strip_prefix = "nghttp2-1.47.0",
    url = "https://github.com/nghttp2/nghttp2/releases/download/v1.47.0/nghttp2-1.47.0.tar.gz",
)

http_archive(
    name = "curl",
    build_file = "//build_defs/cc:curl.BUILD",
    sha256 = "ff3e80c1ca6a068428726cd7dd19037a47cc538ce58ef61c59587191039b2ca6",
    strip_prefix = "curl-7.49.1",
    urls = [
        "https://mirror.bazel.build/curl.haxx.se/download/curl-7.49.1.tar.gz",
    ],
)

http_archive(
    name = "com_google_absl",
    # Committed on Nov 3, 2021.
    sha256 = "a4567ff02faca671b95e31d315bab18b42b6c6f1a60e91c6ea84e5a2142112c2",
    strip_prefix = "abseil-cpp-20211102.0",
    urls = [
        "https://github.com/abseil/abseil-cpp/archive/refs/tags/20211102.0.zip",
    ],
)

git_repository(
    name = "boringssl",
    # Committed on Oct 3, 2022
    # https://github.com/google/boringssl/commit/c2837229f381f5fcd8894f0cca792a94b557ac52
    commit = "c2837229f381f5fcd8894f0cca792a94b557ac52",
    remote = "https://github.com/google/boringssl.git",
)

#####################
# GRPC C & GCP APIs #
#####################
# Loads com_github_grpc_grpc.
# Defines @com_github_grpc_grpc before @com_github_googleapis_google_cloud_cpp
# to override the dependenices in @com_github_googleapis_google_cloud_cpp.
http_archive(
    name = "com_github_grpc_grpc",
    sha256 = "e18b16f7976aab9a36c14c38180f042bb0fd196b75c9fd6a20a2b5f934876ad6",
    strip_prefix = "grpc-1.45.2",
    urls = ["https://github.com/grpc/grpc/archive/refs/tags/v1.45.2.tar.gz"],
)

# Builds Google cloud cpp
# --> must be before grpc_deps() and grpc_extra_deps()
# --> and after http_archive com_github_grpc_grpc
http_archive(
    name = "com_github_googleapis_google_cloud_cpp",
    sha256 = "02ea232bbac826e36ad1140a45cfe2f4552e7e1fbb8f86e839fa2fb3620cdd64",
    strip_prefix = "google-cloud-cpp-1.41.0",
    url = "https://github.com/googleapis/google-cloud-cpp/archive/v1.41.0.tar.gz",
)

################################################################################
# Download all http_archives and git_repositories: End
################################################################################

################################################################################
# Download Maven Dependencies: Begin
################################################################################
load("@rules_jvm_external//:defs.bzl", "maven_install")
load("//build_defs/tink:tink_defs.bzl", "TINK_MAVEN_ARTIFACTS")

JACKSON_VERSION = "2.12.2"

AUTO_VALUE_VERSION = "1.7.4"

AWS_SDK_VERSION = "2.17.239"

GOOGLE_GAX_VERSION = "2.4.0"

AUTO_SERVICE_VERSION = "1.0"

maven_install(
    artifacts = [
        "com.amazonaws:aws-lambda-java-core:1.2.1",
        "com.amazonaws:aws-lambda-java-events:3.8.0",
        "com.amazonaws:aws-lambda-java-events-sdk-transformer:3.1.0",
        "com.amazonaws:aws-java-sdk-sqs:1.11.860",
        "com.amazonaws:aws-java-sdk-s3:1.11.860",
        "com.amazonaws:aws-java-sdk-kms:1.11.860",
        "com.amazonaws:aws-java-sdk-core:1.11.860",
        "com.beust:jcommander:1.81",
        "com.fasterxml.jackson.core:jackson-annotations:" + JACKSON_VERSION,
        "com.fasterxml.jackson.core:jackson-core:" + JACKSON_VERSION,
        "com.fasterxml.jackson.core:jackson-databind:" + JACKSON_VERSION,
        "com.fasterxml.jackson.datatype:jackson-datatype-guava:" + JACKSON_VERSION,
        "com.fasterxml.jackson.datatype:jackson-datatype-jsr310:" + JACKSON_VERSION,
        "com.fasterxml.jackson.datatype:jackson-datatype-jdk8:" + JACKSON_VERSION,
        "com.google.acai:acai:1.1",
        "com.google.auto.factory:auto-factory:1.0",
        "com.google.auto.service:auto-service-annotations:" + AUTO_SERVICE_VERSION,
        "com.google.auto.service:auto-service:" + AUTO_SERVICE_VERSION,
        "com.google.auto.value:auto-value-annotations:" + AUTO_VALUE_VERSION,
        "com.google.auto.value:auto-value:" + AUTO_VALUE_VERSION,
        "com.google.code.findbugs:jsr305:3.0.2",
        "com.google.code.gson:gson:2.8.9",
        "com.google.cloud:google-cloud-kms:2.1.2",
        "com.google.cloud:google-cloud-pubsub:1.114.4",
        "com.google.cloud:google-cloud-storage:1.118.0",
        "com.google.cloud:google-cloud-spanner:6.12.2",
        "com.google.cloud:google-cloud-secretmanager:2.2.0",
        "com.google.cloud:google-cloud-compute:1.12.1",
        "com.google.api.grpc:proto-google-cloud-compute-v1:1.12.1",
        "com.google.cloud.functions.invoker:java-function-invoker:1.1.0",
        "com.google.auth:google-auth-library-oauth2-http:1.11.0",
        "com.google.cloud.functions:functions-framework-api:1.0.4",
        "commons-logging:commons-logging:1.1.1",
        "com.google.api:gax:" + GOOGLE_GAX_VERSION,
        "com.google.http-client:google-http-client-jackson2:1.40.0",
        "com.google.protobuf:protobuf-java:" + PROTOBUF_CORE_VERSION,
        "com.google.protobuf:protobuf-java-util:" + PROTOBUF_CORE_VERSION,
        "com.google.cloud:google-cloud-monitoring:3.4.1",
        "com.google.api.grpc:proto-google-cloud-monitoring-v3:3.4.1",
        "com.google.api.grpc:proto-google-common-protos:2.9.2",
        "com.google.protobuf:protobuf-java-util:" + PROTOBUF_CORE_VERSION,
        "com.google.guava:guava:30.1-jre",
        "com.google.guava:guava-testlib:30.1-jre",
        "com.google.inject:guice:5.1.0",
        "com.google.inject.extensions:guice-testlib:5.1.0",
        "com.google.jimfs:jimfs:1.2",
        "com.google.protobuf:protobuf-java:" + PROTOBUF_CORE_VERSION,
        "com.google.protobuf:protobuf-java-util:" + PROTOBUF_CORE_VERSION,
        "com.google.testparameterinjector:test-parameter-injector:1.1",
        "com.google.truth.extensions:truth-java8-extension:1.1.2",
        "com.google.truth.extensions:truth-proto-extension:1.1.2",
        "com.google.truth:truth:1.1.2",
        "com.jayway.jsonpath:json-path:2.5.0",
        "io.github.resilience4j:resilience4j-core:1.7.1",
        "io.github.resilience4j:resilience4j-retry:1.7.1",
        "javax.annotation:javax.annotation-api:1.3.2",
        "javax.inject:javax.inject:1",
        "junit:junit:4.12",
        "org.apache.avro:avro:1.10.2",
        "org.apache.commons:commons-math3:3.6.1",
        "org.apache.httpcomponents:httpcore:4.4.14",
        "org.apache.httpcomponents:httpclient:4.5.13",
        "org.apache.httpcomponents.client5:httpclient5:5.1.3",
        "org.apache.httpcomponents.core5:httpcore5:5.1.4",
        "org.apache.httpcomponents.core5:httpcore5-h2:5.1.4",  # Explicit transitive dependency to avoid https://issues.apache.org/jira/browse/HTTPCLIENT-2222
        "org.apache.logging.log4j:log4j-1.2-api:2.17.0",
        "org.apache.logging.log4j:log4j-core:2.17.0",
        "org.awaitility:awaitility:3.0.0",
        "org.mock-server:mockserver-core:5.11.2",
        "org.mock-server:mockserver-junit-rule:5.11.2",
        "org.mock-server:mockserver-client-java:5.11.2",
        "org.hamcrest:hamcrest-library:1.3",
        "org.mockito:mockito-core:3.11.2",
        "org.mockito:mockito-inline:3.11.2",
        "org.slf4j:slf4j-api:1.7.30",
        "org.slf4j:slf4j-simple:1.7.30",
        "org.slf4j:slf4j-log4j12:1.7.30",
        "org.testcontainers:testcontainers:1.15.3",
        "org.testcontainers:localstack:1.15.3",
        "software.amazon.awssdk:apigateway:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:arns:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:autoscaling:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:aws-sdk-java:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:dynamodb-enhanced:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:dynamodb:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:cloudwatch:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:ec2:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:pricing:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:regions:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:s3:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:aws-core:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:ssm:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:sts:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:sqs:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:url-connection-client:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:utils:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:auth:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:lambda:" + AWS_SDK_VERSION,
    ] + TINK_MAVEN_ARTIFACTS,
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

################################################################################
# Download Maven Dependencies: End
################################################################################

################################################################################
# Download Indirect Dependencies: Begin
################################################################################
# Note: The order of statements in this section is extremely fragile

load("@rules_java//java:repositories.bzl", "rules_java_dependencies", "rules_java_toolchains")

rules_java_dependencies()

rules_java_toolchains()

# Load dependencies for the base workspace.
load("@com_google_differential_privacy//:differential_privacy_deps.bzl", "differential_privacy_deps")

differential_privacy_deps()

#############
# PKG Rules #
#############
load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

############
# Go rules #
############
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

go_rules_dependencies()

go_register_toolchains(version = "1.18")

gazelle_dependencies()

###################
# Container rules #
###################
load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)

container_repositories()

load("@io_bazel_rules_docker//repositories:deps.bzl", container_deps = "deps")

container_deps()

load("@io_bazel_rules_docker//container:container.bzl", "container_pull")

#############
# CPP Rules #
#############
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies", "rules_cc_toolchains")

rules_cc_dependencies()

rules_cc_toolchains()

###############
# Proto rules #
###############
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()

rules_proto_toolchains()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

###########################
# Binary Dev Dependencies #
###########################
load("@com_github_google_rpmpack//:deps.bzl", "rpmpack_dependencies")

rpmpack_dependencies()

# Imports Tink git repo to this workspace as @tink_java. To be used only for
# testing changes not yet published to Maven.
load("//build_defs/tink:tink_defs.bzl", "import_tink_git")

import_tink_git()

###########################
# CC Dependencies #
###########################
# Load indirect dependencies due to
#     https://github.com/bazelbuild/bazel/issues/1943
load("@com_github_googleapis_google_cloud_cpp//bazel:google_cloud_cpp_deps.bzl", "google_cloud_cpp_deps")

google_cloud_cpp_deps()

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
    grpc = True,
)

# Boost
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")

boost_deps()

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

##########
# GRPC C #
##########
# These dependencies from @com_github_grpc_grpc need to be loaded after the
# google cloud deps so that the grpc version can be set by the google cloud deps
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

################################################################################
# Download Indirect Dependencies: End
################################################################################

################################################################################
# Download Containers: Begin
################################################################################

# Distroless image for running Java.
container_pull(
    name = "java_base",
    # Using SHA-256 for reproducibility. The tag is latest-amd64. Latest as of 2023-07-10.
    digest = "sha256:052076466984fd56979c15a9c3b7433262b0ad9aae55bc0c53d1da8ffdd829c3",
    registry = "gcr.io",
    repository = "distroless/java17-debian11",
)

# Distroless image for running C++.
container_pull(
    name = "cc_base",
    registry = "gcr.io",
    repository = "distroless/cc",
    # Using SHA-256 for reproducibility.
    # TODO: use digest instead of tag, currently it's not working.
    tag = "latest",
)

# Distroless image for running statically linked binaries.
container_pull(
    name = "static_base",
    registry = "gcr.io",
    repository = "distroless/static",
    # Using SHA-256 for reproducibility.
    # TODO: use digest instead of tag, currently it's not working.
    tag = "latest",
)

# Needed for reproducibly building AL2 binaries (e.g. //cc/aws/proxy)
container_pull(
    name = "amazonlinux_2",
    # Latest as of 2023-07-10.
    digest = "sha256:d41496df5f949d9b7567512efa42ecc21ec9dd2c49539f7452945ed435b0058a",
    registry = "index.docker.io",
    repository = "amazonlinux",
    tag = "2.0.20230612.0",
)

# Needed for build containers which must execute bazel commands (e.g. //cc/aws/proxy).
http_file(
    name = "bazelisk",
    downloaded_file_path = "bazelisk",
    executable = True,
    sha256 = "84e946ed8537eaaa4d540df338a593e373e70c5ddca9f2f49e1aaf3a04bdd6ca",
    urls = ["https://github.com/bazelbuild/bazelisk/releases/download/v1.14.0/bazelisk-linux-amd64"],
)

# Needed for cc/pbs/deploy/pbs_server/build_defs
container_pull(
    name = "debian_11",
    digest = "sha256:3098a8fda8e7bc6bc92c37aaaa9d46fa0dd93992203ca3f53bb84e1d00ffb796",
    registry = "index.docker.io",
    repository = "amd64/debian",
    tag = "11",
)

container_pull(
    name = "debian_11_runtime",
    digest = "sha256:105e444974ba7de61aef04a3cfab9ca0ba80babfd9da8295f6df546639d9571e",
    registry = "gcr.io",
    repository = "distroless/cc-debian11",
    tag = "debug-nonroot-amd64",
)

# Needed for cc reproducible builds
load("//cc/tools/build:build_container_params.bzl", "CC_BUILD_CONTAINER_REGISTRY", "CC_BUILD_CONTAINER_REPOSITORY", "CC_BUILD_CONTAINER_TAG")

container_pull(
    name = "prebuilt_cc_build_container_image_pull",
    registry = CC_BUILD_CONTAINER_REGISTRY,
    repository = CC_BUILD_CONTAINER_REPOSITORY,
    tag = CC_BUILD_CONTAINER_TAG,
)
################################################################################
# Download Containers: End
################################################################################

http_archive(
    name = "jemalloc",
    build_file = "//build_defs/cc:libjemalloc.BUILD",
    sha256 = "1f35888bad9fd331f5a03445bc1bff808a59378be61fef01e9736179d76f2fab",
    url = "https://github.com/jemalloc/jemalloc/archive/refs/tags/5.3.0.zip",
)
