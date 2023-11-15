load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

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

# Declare explicit protobuf version and hash, to override any implicit dependencies.
# Please update both while upgrading to new versions.
PROTOBUF_CORE_VERSION = "3.19.4"

PROTOBUF_SHA_256 = "3bd7828aa5af4b13b99c191e8b1e884ebfa9ad371b0ce264605d347f135d2568"

##########################
# SDK Dependencies Rules #
##########################

load("//build_defs/cc:sdk.bzl", "sdk_dependencies")

sdk_dependencies(PROTOBUF_CORE_VERSION, PROTOBUF_SHA_256)

#################################
# SCP Shared Dependencies Rules #
#################################

# This bazel file contains all the dependencies in SCP, except the dependencies
# only used in SDK. Eventually, each project will have its own bazel file for
# its dependencies, and this file will be removed.
load("//build_defs:scp_dependencies.bzl", "scp_dependencies")

scp_dependencies(PROTOBUF_CORE_VERSION, PROTOBUF_SHA_256)

######### To gegerate Java interface for SDK #########
load("@com_google_api_gax_java//:repository_rules.bzl", "com_google_api_gax_java_properties")

com_google_api_gax_java_properties(
    name = "com_google_api_gax_java_properties",
    file = "@com_google_api_gax_java//:dependencies.properties",
)

load("@com_google_api_gax_java//:repositories.bzl", "com_google_api_gax_java_repositories")

com_google_api_gax_java_repositories()

load("@io_grpc_grpc_java//:repositories.bzl", "grpc_java_repositories")

grpc_java_repositories()
##########################################################

################################################################################
# Download all http_archives and git_repositories: End
################################################################################

################################################################################
# Download Maven Dependencies: Begin
################################################################################
load("@rules_jvm_external//:defs.bzl", "maven_install")
load("//build_defs/shared:java_grpc.bzl", "GAPIC_GENERATOR_JAVA_VERSION")
load("//build_defs/tink:tink_defs.bzl", "TINK_MAVEN_ARTIFACTS")

JACKSON_VERSION = "2.15.2"

AUTO_VALUE_VERSION = "1.7.4"

AWS_SDK_VERSION = "2.17.239"

GOOGLE_GAX_VERSION = "2.4.0"

AUTO_SERVICE_VERSION = "1.0"

maven_install(
    name = "maven",
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
        "com.google.guava:guava:32.0.1-jre",
        "com.google.guava:guava-testlib:32.0.1-jre",
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
        "software.amazon.awssdk:s3-transfer-manager:2.20.130",
        "software.amazon.awssdk:aws-core:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:ssm:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:sts:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:sqs:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:url-connection-client:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:utils:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:auth:" + AWS_SDK_VERSION,
        "software.amazon.awssdk:lambda:" + AWS_SDK_VERSION,
        "com.google.api:gapic-generator-java:" + GAPIC_GENERATOR_JAVA_VERSION,  # To use generated gRpc Java interface
        "io.grpc:grpc-netty:1.54.0",
    ] + TINK_MAVEN_ARTIFACTS,
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
)

maven_install(
    name = "maven_yaml",
    artifacts = [
        "org.yaml:snakeyaml:1.27",
    ],
    repositories = [
        "https://repo1.maven.org/maven2",
    ],
    # Pin the working version for snakeyaml.
    version_conflict_policy = "pinned",
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
    java = True,
)

# Boost
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")

boost_deps()

# Foreign CC
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

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

############
# Go rules #
############
# Need to be after grpc_extra_deps to share go_register_toolchains.
load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies")

go_rules_dependencies()

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

###########################
# Binary Dev Dependencies #
###########################
load("@com_github_google_rpmpack//:deps.bzl", "rpmpack_dependencies")
load("@io_bazel_rules_docker//container:container.bzl", "container_pull")

rpmpack_dependencies()

################################################################################
# Download Containers: Begin
################################################################################

load("//build_defs:container_dependencies.bzl", container_dependencies = "CONTAINER_DEPS")

[
    container_pull(
        name = img_name,
        digest = img_info["digest"],
        registry = img_info["registry"],
        repository = img_info["repository"],
    )
    for img_name, img_info in container_dependencies.items()
]

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

########################################################################
# Roma dependencies
load("//build_defs/cc:roma.bzl", "roma_dependencies")

roma_dependencies()

load(
    "@com_google_sandboxed_api//sandboxed_api/bazel:llvm_config.bzl",
    "llvm_disable_optional_support_deps",
)

# Must be right after roma_dependencies
load(
    "@com_google_sandboxed_api//sandboxed_api/bazel:sapi_deps.bzl",
    "sapi_deps",
)

llvm_disable_optional_support_deps()

sapi_deps()
########################################################################

# Needed for cc reproducible builds
load("//cc/tools/build:build_container_params.bzl", "CC_BUILD_CONTAINER_REGISTRY", "CC_BUILD_CONTAINER_REPOSITORY", "CC_BUILD_CONTAINER_TAG")

container_pull(
    name = "prebuilt_cc_build_container_image_pull",
    registry = CC_BUILD_CONTAINER_REGISTRY,
    repository = CC_BUILD_CONTAINER_REPOSITORY,
    tag = CC_BUILD_CONTAINER_TAG,
)

##########################
## Closure dependencies ##
##########################
load("//build_defs/shared:bazel_rules_closure.bzl", "bazel_rules_closure")

bazel_rules_closure()

load(
    "@io_bazel_rules_closure//closure:repositories.bzl",
    "rules_closure_dependencies",
    "rules_closure_toolchains",
)

rules_closure_dependencies()

rules_closure_toolchains()

# Needed for cc/pbs/deploy/pbs_server/build_defs
container_pull(
    name = "debian_11",
    digest = "sha256:9d23db14fbdc095689c423af56b9525538de139a1bbe950b4f0467698fb874d2",
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

################################################################################
# Download Containers: End
################################################################################

http_archive(
    name = "jemalloc",
    build_file = "//build_defs/cc/shared/build_targets:libjemalloc.BUILD",
    sha256 = "1f35888bad9fd331f5a03445bc1bff808a59378be61fef01e9736179d76f2fab",
    url = "https://github.com/jemalloc/jemalloc/archive/refs/tags/5.3.0.zip",
)

#########################
## NodeJS dependencies ##
#########################

http_archive(
    name = "build_bazel_rules_nodejs",
    sha256 = "94070eff79305be05b7699207fbac5d2608054dd53e6109f7d00d923919ff45a",
    urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/5.8.2/rules_nodejs-5.8.2.tar.gz"],
)

load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")

build_bazel_rules_nodejs_dependencies()

load("@build_bazel_rules_nodejs//:index.bzl", "npm_install")

npm_install(
    name = "npm",
    package_json = "//typescript/coordinator/aws/adtechmanagement:package.json",
    package_lock_json = "//typescript/coordinator/aws/adtechmanagement:package-lock.json",
)

#######################
## Python dependencies #
#######################

load("@rules_python//python:pip.bzl", "pip_install")

# Separate test dependencies are needed for GCP Auth lambda because the regular requirements.txt
# install the cloud spanner dependency and it times out during local builds. This spanner dependency is only needed
# for actual code and not for unit tests. Creating a separate test dependencies bundle allows pulling only those deps
# needed for tests during local builds
pip_install(
    name = "py3_privacybudget_gcp_pbs_auth_handler_test_deps",
    requirements = "//:python/privacybudget/gcp/pbs_auth_handler/config/test_requirements.txt",
)

pip_install(
    name = "py3_privacybudget_aws_pbs_auth_handler_deps",
    requirements = "//:python/privacybudget/aws/pbs_auth_handler/requirements.txt",
)

#######################
## rules_esbuild setup #
#######################

load("@build_bazel_rules_nodejs//toolchains/esbuild:esbuild_repositories.bzl", "esbuild_repositories")

esbuild_repositories(npm_repository = "npm")
