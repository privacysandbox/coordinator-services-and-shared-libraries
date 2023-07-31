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

package com.google.scp.shared.clients.configclient.aws;

import static com.google.common.collect.MoreCollectors.toOptional;
import static com.google.scp.shared.clients.configclient.model.ErrorReason.FETCH_ERROR;
import static com.google.scp.shared.clients.configclient.model.WorkerParameter.COORDINATOR_A_ROLE;
import static com.google.scp.shared.clients.configclient.model.WorkerParameter.COORDINATOR_B_ROLE;
import static com.google.scp.shared.clients.configclient.model.WorkerParameter.COORDINATOR_KMS_ARN;
import static com.google.scp.shared.clients.configclient.model.WorkerParameter.JOB_METADATA_DB;
import static com.google.scp.shared.clients.configclient.model.WorkerParameter.JOB_QUEUE;
import static com.google.scp.shared.clients.configclient.model.WorkerParameter.MAX_JOB_NUM_ATTEMPTS;
import static com.google.scp.shared.clients.configclient.model.WorkerParameter.MAX_JOB_PROCESSING_TIME_SECONDS;
import static com.google.scp.shared.clients.configclient.model.WorkerParameter.SCALE_IN_HOOK;

import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.google.common.cache.CacheBuilder;
import com.google.common.cache.CacheLoader;
import com.google.common.cache.LoadingCache;
import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClientUtils;
import com.google.scp.shared.clients.configclient.model.WorkerParameter;
import java.util.Optional;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.inject.Inject;
import javax.inject.Singleton;
import software.amazon.awssdk.core.exception.SdkClientException;
import software.amazon.awssdk.regions.internal.util.EC2MetadataUtils;
import software.amazon.awssdk.services.ec2.Ec2Client;
import software.amazon.awssdk.services.ec2.model.DescribeTagsRequest;
import software.amazon.awssdk.services.ec2.model.DescribeTagsResponse;
import software.amazon.awssdk.services.ec2.model.Filter;
import software.amazon.awssdk.services.ssm.SsmClient;
import software.amazon.awssdk.services.ssm.model.GetParametersRequest;
import software.amazon.awssdk.services.ssm.model.GetParametersResponse;
import software.amazon.awssdk.services.ssm.model.Parameter;
import software.amazon.awssdk.services.ssm.model.SsmException;

/** Parameter client implementation for getting parameters from AWS parameter store. */
@Singleton
public final class AwsParameterClient implements ParameterClient {

  private static final Logger logger = Logger.getLogger(AwsParameterClient.class.getName());
  private static final int MAX_CACHE_SIZE = 100;
  private static final long CACHE_ENTRY_TTL_SEC = 3600;
  private static final String DEFAULT_PARAM_PREFIX = "scp";
  private static final String ENVIRONMENT_TAG_NAME = "environment";

  private static final ImmutableMap<WorkerParameter, String> LEGACY_PARAMETER_NAMES =
      ImmutableMap.<WorkerParameter, String>builder()
          .put(JOB_QUEUE, "/aggregate-service/%s/sqs_queue_url")
          .put(JOB_METADATA_DB, "/aggregate-service/%s/dynamodb_metadatadb_table_name")
          .put(MAX_JOB_NUM_ATTEMPTS, "/aggregate-service/%s/max_job_num_attempts")
          .put(
              MAX_JOB_PROCESSING_TIME_SECONDS,
              "/aggregate-service/%s/max_job_processing_time_seconds")
          .put(COORDINATOR_A_ROLE, "/aggregate-service/%s/coordinator_a_assume_role_arn")
          .put(COORDINATOR_B_ROLE, "/aggregate-service/%s/coordinator_b_assume_role_arn")
          .put(COORDINATOR_KMS_ARN, "/aggregate-service/%s/kms_key_arn")
          .put(SCALE_IN_HOOK, "/aggregate-service/%s/scale_in_hook")
          .build();

  private final Ec2Client ec2Client;
  private final SsmClient ssmClient;

  private final Supplier<String> parameterEnvSupplier =
      Suppliers.memoizeWithExpiration(
          this::loadParameterEnv, CACHE_ENTRY_TTL_SEC, TimeUnit.SECONDS);
  private final LoadingCache<String, Optional<String>> paramCache =
      CacheBuilder.newBuilder()
          .maximumSize(MAX_CACHE_SIZE)
          .expireAfterWrite(CACHE_ENTRY_TTL_SEC, TimeUnit.SECONDS)
          .build(
              new CacheLoader<String, Optional<String>>() {
                @Override
                public Optional<String> load(String key) throws Exception {
                  return getParameterValue(key);
                }
              });

  /** Creates a new instance of the {@code AwsParameterClient} class. */
  @Inject
  AwsParameterClient(Ec2Client ec2Client, SsmClient ssmClient) {
    this.ec2Client = ec2Client;
    this.ssmClient = ssmClient;
  }

  @Override
  public Optional<String> getParameter(String param) throws ParameterClientException {
    return getParameter(
        param, Optional.of(DEFAULT_PARAM_PREFIX), /* includeEnvironmentParam= */ true);
  }

  @Override
  public Optional<String> getParameter(
      String param, Optional<String> paramPrefix, boolean includeEnvironmentParam)
      throws ParameterClientException {
    Optional<String> environment =
        includeEnvironmentParam ? getEnvironmentName() : Optional.empty();
    String storageParam =
        ParameterClientUtils.getStorageParameterName(param, paramPrefix, environment);

    Optional<String> paramValue;
    try {
      paramValue = paramCache.get(storageParam);
    } catch (ExecutionException e) {
      logger.log(Level.SEVERE, "Error getting value for AWS parameter " + storageParam, e);
      throw new ParameterClientException(
          "Error getting value for AWS parameter " + storageParam, FETCH_ERROR, e);
    }
    if (paramValue.isPresent()) {
      return paramValue;
    }

    // No parameter value was found. For safety, check if a value can be found using the legacy
    // parameter name. This may happen if the new parameter names have not yet been pushed for an
    // environment during the migration.
    // TODO(b/231332179): Remove this once all new parameters are pushed to all environments.
    Optional<String> legacyStorageParamOpt = getLegacyStorageParameterName(param);
    if (legacyStorageParamOpt.isEmpty()) {
      return paramValue;
    }

    String legacyStorageParam = legacyStorageParamOpt.get();
    Optional<String> legacyParamValue;
    try {
      legacyParamValue = paramCache.get(legacyStorageParam);
    } catch (ExecutionException e) {
      logger.log(
          Level.SEVERE, "Error getting value for legacy AWS parameter " + legacyStorageParam, e);
      throw new ParameterClientException(
          "Error getting value for legacy AWS parameter " + legacyStorageParam, FETCH_ERROR, e);
    }

    legacyParamValue.ifPresent(
        unused ->
            logger.log(
                Level.SEVERE,
                String.format(
                    "Found value for legacy storage parameter '%s', but missing value for '%s'.",
                    legacyStorageParam, storageParam)));
    return legacyParamValue;
  }

  @Override
  public Optional<String> getEnvironmentName() {
    return Optional.ofNullable(parameterEnvSupplier.get());
  }

  /**
   * Builds the legacy parameter name used by the underlying parameter storage layer.
   *
   * <p>The name is formatted as `/aggregate-service/{environment}/{parameter}`, where the parameter
   * fragment may or may not be the same as the parameter value provided.
   *
   * @param param the name of the parameter
   * @return the fully expanded legacy storage parameter name
   */
  private Optional<String> getLegacyStorageParameterName(String param) {
    WorkerParameter workerParameter;
    try {
      workerParameter = WorkerParameter.valueOf(param);
    } catch (IllegalArgumentException | NullPointerException e) {
      return Optional.empty();
    }

    return Optional.ofNullable(LEGACY_PARAMETER_NAMES.get(workerParameter))
        .flatMap(paramPattern -> getEnvironmentName().map(env -> String.format(paramPattern, env)));
  }

  /**
   * Gets the parameter environment name from instance tags
   *
   * <p>There must be a tag with name {@code ENVIRONMENT_TAG_NAME} set for the EC2 instance, and the
   * value of this tag will be used as the parameter environment name.
   */
  private String loadParameterEnv() {
    String instanceId = EC2MetadataUtils.getInstanceId();
    DescribeTagsRequest request =
        DescribeTagsRequest.builder()
            .filters(
                Filter.builder().name("resource-id").values(instanceId).build(),
                Filter.builder().name("key").values(ENVIRONMENT_TAG_NAME).build())
            .build();
    DescribeTagsResponse response = ec2Client.describeTags(request);
    logger.info(String.format("Received tags: %s", response.toString()));
    if (response.tags().size() != 1) {
      // TODO(b/197900057): throw checked exception, currently Suppliers.memoize prevent that, but
      // with custom functional interface it may be resolved.
      throw new IllegalStateException(
          String.format(
              "Could not find tag '%s' associated with instance %s.",
              ENVIRONMENT_TAG_NAME, instanceId));
    }

    return response.tags().get(0).value();
  }

  private Optional<String> getParameterValue(String param) throws ParameterClientException {
    try {
      GetParametersRequest request = GetParametersRequest.builder().names(param).build();
      logger.fine(String.format("Get parameter request: %s.", request.toString()));

      GetParametersResponse resp = ssmClient.getParameters(request);
      logger.fine(
          String.format(
              "Fetched parameters from AWS parameter store, has parameters: %b, has invalid"
                  + " parameters flag set by server: %b.",
              resp.hasParameters(), resp.hasInvalidParameters()));

      Optional<String> value =
          resp.parameters().stream()
              .filter(p -> param.equals(p.name()))
              .map(Parameter::value)
              .collect(toOptional());
      logger.info(String.format("Loaded parameter %s: %s.", param, value));
      return value;
    } catch (SdkClientException | SsmException e) {
      throw new ParameterClientException(
          String.format("Error getting value of %s from AWS parameter store.", param),
          FETCH_ERROR,
          e);
    }
  }
}
