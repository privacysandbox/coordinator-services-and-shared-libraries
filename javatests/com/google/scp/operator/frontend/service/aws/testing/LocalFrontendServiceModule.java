/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.operator.frontend.service.aws.testing;

import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.addLambdaHandler;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.getLocalStackHostname;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.uploadLambdaCode;

import com.google.acai.BeforeSuite;
import com.google.acai.TestingService;
import com.google.acai.TestingServiceModule;
import com.google.common.collect.ImmutableMap;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb.MetadataDbDynamoTableName;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb.MetadataDbDynamoTtlDays;
import com.google.scp.operator.shared.injection.modules.EnvironmentVariables;
import java.nio.file.Path;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.s3.S3Client;

/**
 * Uses the Lambda API to deploy a local version of the frontend service, which can be invoked by
 * name ({@link #GET_JOB_LAMBDA_NAME} and {@link #CREATE_JOB_LAMBDA_NAME}) using the Lambda API.
 * Intended to be used with LocalStack.
 */
public final class LocalFrontendServiceModule extends AbstractModule {

  /** Lambda function for the GetJob service. */
  public static final String GET_JOB_LAMBDA_NAME = "integration_get_job_lambda";

  /** Lambda function for the CreateJob service. */
  public static final String CREATE_JOB_LAMBDA_NAME = "integration_create_job_lambda";

  // LINT.IfChange(integration-frontend-deploy-jar)
  private static final Path jarPath =
      Path.of("java/com/google/scp/operator/frontend/AwsApigatewayFrontend_deploy.jar");
  // LINT.ThenChange(BUILD:integration-frontend-deploy-jar)
  private static final String GET_JOB_HANDLER_CLASS =
      "com.google.scp.operator.frontend.service.aws.GetJobApiGatewayHandler";
  private static final String CREATE_JOB_HANDLER_CLASS =
      "com.google.scp.operator.frontend.service.aws.CreateJobApiGatewayHandler";
  private static final String LAMBDA_JAR_KEY = "app/integration_frontend_lambda.jar";

  /** Configures injected dependencies for this module. */
  @Override
  public void configure() {
    install(TestingServiceModule.forServices(GetJobLambdaService.class));
  }

  private static class GetJobLambdaService implements TestingService {
    @Inject LocalStackContainer localStack;
    @Inject LambdaClient lambdaClient;
    @Inject S3Client s3Client;
    @Inject @MetadataDbDynamoTableName String table;
    @Inject @MetadataDbDynamoTtlDays Integer ttl;

    @BeforeSuite
    void createLambda() {
      uploadLambdaCode(s3Client, LAMBDA_JAR_KEY, jarPath);

      addLambdaHandler(
          lambdaClient,
          GET_JOB_LAMBDA_NAME,
          GET_JOB_HANDLER_CLASS,
          LAMBDA_JAR_KEY,
          ImmutableMap.of(
              EnvironmentVariables.AWS_ENDPOINT_URL_ENV_VAR,
              String.format("http://%s:4566", getLocalStackHostname(localStack)),
              EnvironmentVariables.JOB_METADATA_TABLE_ENV_VAR,
              table,
              EnvironmentVariables.JOB_METADATA_TTL_ENV_VAR,
              ttl.toString()));

      addLambdaHandler(
          lambdaClient,
          CREATE_JOB_LAMBDA_NAME,
          CREATE_JOB_HANDLER_CLASS,
          LAMBDA_JAR_KEY,
          ImmutableMap.of(
              EnvironmentVariables.AWS_ENDPOINT_URL_ENV_VAR,
              String.format("http://%s:4566", getLocalStackHostname(localStack)),
              EnvironmentVariables.JOB_METADATA_TABLE_ENV_VAR,
              table,
              EnvironmentVariables.JOB_METADATA_TTL_ENV_VAR,
              ttl.toString()));
    }
  }
}
