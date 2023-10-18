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

package com.google.scp.coordinator.keymanagement.keyhosting.service.aws;

import static com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbRegion;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;

import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.Annotations.CacheControlMaximum;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.Annotations.DisableActivationTime;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDbModule;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import java.util.Map;
import java.util.Optional;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.EnvironmentVariableCredentialsProvider;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.http.urlconnection.UrlConnectionHttpClient;

/**
 * Defines dependencies for AWS implementation of KeyService. It can be used for both Public Key and
 * Private Key API Gateway Handler.
 *
 * <p>Intended to be used with Lambda Functions configured with environment variables.
 */
public final class AwsKeyServiceModule extends AbstractModule {
  /** Environment variable for KeyDB table name. */
  private static final String KEYSTORE_TABLE_ENV_VAR = "KEYSTORE_TABLE_NAME";
  /** Environment variable containing AWS region. */
  private static final String AWS_REGION_ENV_VAR = "AWS_REGION";
  /** Environment variable containing region of KeyDb, if not in same region as lambda. */
  private static final String KEY_DB_REGION_ENV_VAR = "KEY_DB_REGION";
  /** Environment variable to define number of keys returned in GetActivePublicKeysResponse. */
  private static final String KEY_LIMIT_ENV_VAR = "KEY_LIMIT";

  /** Environment variable containing cache control maximum in seconds. */
  private static final String CACHE_CONTROL_MAXIMUM_ENV_VAR = "CACHE_CONTROL_MAXIMUM";

  /** Environment variable disabling activationTime field in Encryption Key proto. */
  private static final String DISABLE_ACTIVATION_TIME_VAR = "DISABLE_ACTIVATION_TIME";

  /** Environment variable containing endpoint override for DDB. Used for testing. */
  private static final String ENDPOINT_OVERRIDE_ENV_VAR = "KEYSTORE_ENDPOINT_OVERRIDE";

  /**
   * Returns name for DynamoDb table containing keys. Populated from Environment variable defined in
   * coordinator/terraform/aws/services/keydb/main.tf
   */
  private static String getDynamoKeyDbTableName() {
    // TODO: Replace getOrDefault with thrown Exception if not in vars.
    Map<String, String> env = System.getenv();
    return env.getOrDefault(KEYSTORE_TABLE_ENV_VAR, "unspecified_table");
  }

  /** Returns KeyLimit as Integer from environment var. Default value of 5. */
  private static Integer getKeyLimit() {
    Map<String, String> env = System.getenv();
    return Integer.valueOf(env.getOrDefault(KEY_LIMIT_ENV_VAR, "5"));
  }

  /**
   * Returns CACHE_CONTROL_MAXIMUM as long from environment var. This value should reflect the key
   * generation rate. Default value of 7 days in seconds.
   */
  private static Long getCacheControlMaximum() {
    Map<String, String> env = System.getenv();
    return Long.valueOf(env.getOrDefault(CACHE_CONTROL_MAXIMUM_ENV_VAR, "604800"));
  }

  /**
   * Returns DISABLE_ACTIVATION_TIME_VAR as boolean from environment var.
   * Default value is false.
   */
  private static Boolean getDisableActivationTime() {
    Map<String, String> env = System.getenv();
    return Boolean.valueOf(env.getOrDefault(DISABLE_ACTIVATION_TIME_VAR, "false"));
  }

  @Override
  protected void configure() {
    bind(SdkHttpClient.class).toInstance(UrlConnectionHttpClient.builder().build());
    // Lambda performance optimization: define explicit credential provider
    bind(AwsCredentialsProvider.class).toInstance(EnvironmentVariableCredentialsProvider.create());
    Map<String, String> env = System.getenv();
    bind(String.class)
        .annotatedWith(DynamoKeyDbTableName.class)
        .toInstance(getDynamoKeyDbTableName());
    bind(String.class)
        .annotatedWith(DynamoKeyDbRegion.class)
        .toInstance(env.getOrDefault(KEY_DB_REGION_ENV_VAR, env.get(AWS_REGION_ENV_VAR)));
    install(new DynamoKeyDbModule(Optional.ofNullable(env.get(ENDPOINT_OVERRIDE_ENV_VAR))));
    bind(Integer.class).annotatedWith(KeyLimit.class).toInstance(getKeyLimit());
    bind(Long.class).annotatedWith(CacheControlMaximum.class).toInstance(getCacheControlMaximum());
    bind(KeyDb.class).to(DynamoKeyDb.class);
    bind(Boolean.class).annotatedWith(DisableActivationTime.class).toInstance(getDisableActivationTime());
  }
}
