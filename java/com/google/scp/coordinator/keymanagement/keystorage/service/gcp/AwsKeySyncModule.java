/*
 * Copyright 2025 Google LLC
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

package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import com.google.crypto.tink.Aead;
import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.AwsKeySyncKeyAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.AwsKeySyncKeyDb;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.AwsKeySyncKeyUri;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.util.GcpAeadProvider;
import com.google.scp.shared.aws.credsprovider.FederatedAwsCredentialsProvider;
import java.util.Map;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.http.apache.ApacheHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;

public class AwsKeySyncModule extends AbstractModule {

  private static final String AWS_KEY_SYNC_KMS_KEY_URI_ENV_VAR = "AWS_KEY_SYNC_KMS_KEY_URI";
  private static final String AWS_KEY_SYNC_ROLE_ARN_ENV_VAR = "AWS_KEY_SYNC_ROLE_ARN";
  private static final String AWS_KEY_SYNC_KEYDB_REGION_ENV_VAR = "AWS_KEY_SYNC_KEYDB_REGION";
  private static final String AWS_KEY_SYNC_KEYDB_TABLE_NAME_ENV_VAR =
      "AWS_KEY_SYNC_KEYDB_TABLE_NAME";
  Map<String, String> env;

  AwsKeySyncModule(Map<String, String> env) {
    this.env = env;
  }

  private KeyDb getAwsKeySyncDynamodb(String tableName, String awsRegion, String roleArn) {
    DynamoDbClient client =
        DynamoDbClient.builder()
            .credentialsProvider(new FederatedAwsCredentialsProvider(roleArn))
            .httpClient(ApacheHttpClient.builder().build())
            .region(Region.of(awsRegion))
            .build();
    DynamoDbEnhancedClient enhancedClient =
        DynamoDbEnhancedClient.builder().dynamoDbClient(client).build();
    return new DynamoKeyDb(enhancedClient, tableName);
  }

  @Override
  protected void configure() {
    String awsKeySyncKmsUri = env.get(AWS_KEY_SYNC_KMS_KEY_URI_ENV_VAR);
    String awsKeySyncRoleArn = env.get(AWS_KEY_SYNC_ROLE_ARN_ENV_VAR);
    String awsKeySyncDynamodbRegion = env.get(AWS_KEY_SYNC_KEYDB_REGION_ENV_VAR);
    String awsKeySyncDynamodbTableName = env.get(AWS_KEY_SYNC_KEYDB_TABLE_NAME_ENV_VAR);

    bind(String.class).annotatedWith(AwsKeySyncKeyUri.class).toInstance(awsKeySyncKmsUri);
    bind(Aead.class)
        .annotatedWith(AwsKeySyncKeyAead.class)
        .toInstance(GcpAeadProvider.getAwsAead(awsKeySyncKmsUri, awsKeySyncRoleArn));
    bind(KeyDb.class)
        .annotatedWith(AwsKeySyncKeyDb.class)
        .toInstance(
            getAwsKeySyncDynamodb(
                awsKeySyncDynamodbTableName, awsKeySyncDynamodbRegion, awsKeySyncRoleArn));
  }
}
