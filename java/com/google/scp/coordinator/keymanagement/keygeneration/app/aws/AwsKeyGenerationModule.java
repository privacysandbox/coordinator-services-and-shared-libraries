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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws;

import static com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbRegion;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsV2Client;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationKeyCount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationTtlInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationValidityInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDbModule;
import java.security.GeneralSecurityException;
import java.util.Map;
import java.util.Optional;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.EnvironmentVariableCredentialsProvider;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.http.urlconnection.UrlConnectionHttpClient;

/**
 * Module for providing the values needed by the Key Generation Lambda function, retrieving
 * configuration values from environment variables.
 *
 * @deprecated Single-party key features are deprecated. Pending removal b/282204533.
 */
@Deprecated
public final class AwsKeyGenerationModule extends AbstractModule {

  /** Environment variable containing table name of KeyDb */
  private static final String KEYSTORE_TABLE_NAME_ENV_VAR = "KEYSTORE_TABLE_NAME";

  /** Environment variable containing region of KeyDb, if not in same region as lambda */
  private static final String KEYSTORE_TABLE_REGION_ENV_VAR = "KEYSTORE_TABLE_REGION";

  /** Environment variable containing KMS Encryption Key ARN */
  private static final String ENCRYPTION_KEY_ARN_ENV_VAR = "ENCRYPTION_KEY_ARN";

  /** Environment variable containing endpoint override for DDB. */
  private static final String KEYSTORE_ENDPOINT_OVERRIDE_ENV_VAR = "KEYSTORE_ENDPOINT_OVERRIDE";

  /** Environment variable containing number of keys to generate at a time */
  private static final String KEY_GENERATION_KEY_COUNT_ENV_VAR = "KEY_GENERATION_KEY_COUNT";

  /**
   * Environment variable containing number of days keys will be valid. Should be one more than key
   * generation rate for failover validity
   */
  private static final String KEY_GENERATION_VALIDITY_IN_DAYS_ENV_VAR =
      "KEY_GENERATION_VALIDITY_IN_DAYS";

  /**
   * Environment variable containing number of days after creation to apply a Time-to-Live for keys.
   * Keys will be deleted from the database this number of days after creation time.
   */
  private static final String KEY_GENERATION_TTL_IN_DAYS_ENV_VAR = "KEY_GENERATION_TTL_IN_DAYS";

  /** Environment variable map. */
  private static final Map<String, String> ENV = System.getenv();

  /** Returns the configured URI of the AWS encryption key. */
  private static String getMasterKeyUri() {
    String keyArn = ENV.getOrDefault(ENCRYPTION_KEY_ARN_ENV_VAR, "unspecified_key_id");
    return AwsKmsV2Client.PREFIX + keyArn;
  }

  /** Returns number of keys to generate at a time */
  private static Integer getKeyGenerationKeyCountEnvVar() {
    return Integer.valueOf(ENV.getOrDefault(KEY_GENERATION_KEY_COUNT_ENV_VAR, "5"));
  }

  /** Returns number of days keys will be valid */
  private static Integer getKeyGenerationValidityInDays() {
    return Integer.valueOf(ENV.getOrDefault(KEY_GENERATION_VALIDITY_IN_DAYS_ENV_VAR, "8"));
  }

  /** Returns number of days to add to key Time-to-Live */
  private static Integer getKeyGenerationTtlInDays() {
    return Integer.valueOf(ENV.getOrDefault(KEY_GENERATION_TTL_IN_DAYS_ENV_VAR, "365"));
  }

  @Override
  protected void configure() {
    bind(String.class).annotatedWith(KeyEncryptionKeyUri.class).toInstance(getMasterKeyUri());
    bind(SdkHttpClient.class).toInstance(UrlConnectionHttpClient.builder().build());
    bind(AwsCredentialsProvider.class).toInstance(EnvironmentVariableCredentialsProvider.create());
    bind(Integer.class)
        .annotatedWith(KeyGenerationKeyCount.class)
        .toInstance(getKeyGenerationKeyCountEnvVar());
    bind(Integer.class)
        .annotatedWith(KeyGenerationValidityInDays.class)
        .toInstance(getKeyGenerationValidityInDays());
    bind(Integer.class)
        .annotatedWith(KeyGenerationTtlInDays.class)
        .toInstance(getKeyGenerationTtlInDays());
    bind(String.class)
        .annotatedWith(DynamoKeyDbTableName.class)
        .toInstance(ENV.getOrDefault(KEYSTORE_TABLE_NAME_ENV_VAR, "unspecified_table"));
    bind(String.class)
        .annotatedWith(DynamoKeyDbRegion.class)
        .toInstance(ENV.get(KEYSTORE_TABLE_REGION_ENV_VAR));
    install(
        new DynamoKeyDbModule(Optional.ofNullable(ENV.get(KEYSTORE_ENDPOINT_OVERRIDE_ENV_VAR))));
  }

  @Provides
  @KmsKeyAead
  public Aead provideAead(SdkHttpClient httpClient, AwsCredentialsProvider credentialsProvider) {
    try {
      return new AwsKmsV2Client()
          .withHttpClient(httpClient)
          .withCredentialsProvider(credentialsProvider)
          .getAead(getMasterKeyUri());
    } catch (GeneralSecurityException e) {
      throw new AssertionError("Failed to build AwsKmsV2Client", e);
    }
  }
}
