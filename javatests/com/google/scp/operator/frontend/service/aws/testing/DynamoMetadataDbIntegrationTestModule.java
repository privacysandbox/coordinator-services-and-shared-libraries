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

import com.google.acai.BeforeTest;
import com.google.acai.TestingService;
import com.google.acai.TestingServiceModule;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb.MetadataDbDynamoClient;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb.MetadataDbDynamoTableName;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb.MetadataDbDynamoTtlDays;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import java.time.Clock;
import java.util.UUID;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.DeleteTableRequest;

/** Module for integration testing with a local DynamoDB metadata DB. */
public final class DynamoMetadataDbIntegrationTestModule extends AbstractModule {

  /** Randomized name to allow for use against a shared resource (e.g. prod AWS DynamoDB). */
  public static final String TABLE_NAME = "MetadataDBIntegrationTest_" + UUID.randomUUID();

  /**
   * Provides a singleton instance of the {@code DynamoDbEnhancedClient} class with the {@code
   * MetadataDbDynamoClient} annotation.
   */
  @Provides
  @Singleton
  @MetadataDbDynamoClient
  public DynamoDbEnhancedClient getDynamoDbEnhancedClient(DynamoDbClient dynamoDbClient) {
    return DynamoDbEnhancedClient.builder().dynamoDbClient(dynamoDbClient).build();
  }

  /** Configures injected dependencies for this module. */
  @Override
  public void configure() {
    install(TestingServiceModule.forServices(MetadataDbCleanupService.class));
    bind(JobMetadataDb.class).to(DynamoMetadataDb.class);
    bind(String.class).annotatedWith(MetadataDbDynamoTableName.class).toInstance(TABLE_NAME);
    bind(Integer.class).annotatedWith(MetadataDbDynamoTtlDays.class).toInstance(365);
    bind(Clock.class).toInstance(Clock.systemUTC());
  }

  /** Used to create a service to create and destroy the metadata DB before and after tests. */
  public static class MetadataDbCleanupService implements TestingService {
    @Inject @MetadataDbDynamoTableName private String metadataDbTableName;
    @Inject private DynamoDbClient ddbClient;

    @BeforeTest
    void createTable() {
      try {
        // Attempt to delete the table in @BeforeTest (ignoring exceptions when it doesn't exist)
        // because @AfterTest isn't guaranteed to run, which can lead to leaked state
        ddbClient.deleteTable(DeleteTableRequest.builder().tableName(metadataDbTableName).build());
      } catch (Exception e) {
      }
      AwsIntegrationTestUtil.createMetadataDbTable(ddbClient, metadataDbTableName);
    }
  }
}
