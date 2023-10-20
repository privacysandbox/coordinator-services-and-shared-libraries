/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.privacy.budgeting.service.aws;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.auto.service.AutoService;
import com.google.common.base.Strings;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.privacy.budgeting.dao.aws.AwsPrivacyBudgetDatabase;
import com.google.scp.coordinator.privacy.budgeting.dao.aws.model.converter.AwsPrivacyBudgetConverter;
import com.google.scp.coordinator.privacy.budgeting.dao.common.PrivacyBudgetDatabaseBridge;
import com.google.scp.coordinator.privacy.budgeting.dao.model.converter.PrivacyBudgetUnitConverter;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.AdTechIdentityDatabaseClient;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.AdTechIdentityDatabaseTableName;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetDatabaseClient;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetDatabaseTableName;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetObjectMapper;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetTtlBuffer;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.RequestAuthEnabled;
import com.google.scp.coordinator.privacy.budgeting.service.common.PrivacyBudgetingServiceModule;
import com.google.scp.shared.mapper.TimeObjectMapper;
import java.net.URI;
import java.util.Optional;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.http.apache.ApacheHttpClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClientBuilder;

// TODO: Decide what is the best way to split to modules up between cloud providers. Currently, the
// plan is to build the individual modules into the JAR files.
// TODO(b/233653821) Split up into sub-modules for dao components
/**
 * The production Guice module for the privacy budgeting service in AWS. This wires all the
 * dependencies that are needed for the service.
 */
@AutoService(PrivacyBudgetingServiceModule.class)
public final class AwsPrivacyBudgetingServiceModule extends PrivacyBudgetingServiceModule {

  private static final String PRIVACY_BUDGET_TABLE_NAME_ENV_VAR = "PRIVACY_BUDGET_TABLE_NAME";
  private static final String AD_TECH_IDENTITY_TABLE_NAME_ENV_VAR = "AD_TECH_IDENTITY_TABLE_NAME";
  private static final String AUTH_ENABLED_ENV_VAR = "AUTH_ENABLED";

  private String privacyBudgetDatabaseTableName() {
    return System.getenv(PRIVACY_BUDGET_TABLE_NAME_ENV_VAR);
  }

  /**
   * Retrieves the identity DB table name from an env var. Returns empty string if the environment
   * variable isn't provided.
   */
  private String adTechIdentityDatabaseTableName() {
    return Optional.ofNullable(System.getenv(AD_TECH_IDENTITY_TABLE_NAME_ENV_VAR)).orElse("");
  }

  private boolean authEnabled() {
    String authEnabled = System.getenv(AUTH_ENABLED_ENV_VAR);
    if (!Strings.isNullOrEmpty(authEnabled)) {
      return Boolean.parseBoolean(authEnabled);
    }
    // TODO(b/233660071) enable auth by default
    return false; // Default to auth disabled if no value is set
  }

  /**
   * Provides the dynamo DB client used for both the privacy budget db and the identity db.
   *
   * <p>This is a {@link Singleton} as AWS recommends making only one client that is used throughout
   * the application.
   */
  @Provides
  @Singleton
  @PrivacyBudgetDatabaseClient
  DynamoDbClient provideDynamoDbClient() {
    final String endpoint = System.getenv("DYNAMODB_ENDPOINT_OVERRIDE");
    DynamoDbClientBuilder builder = DynamoDbClient.builder();
    builder.httpClient(ApacheHttpClient.builder().build());
    if (!Strings.isNullOrEmpty(endpoint)) {
      builder.endpointOverride(URI.create(endpoint));
    }
    return builder.build();
  }

  /**
   * DynamoDbEnhanced client to be used in the identity db.
   *
   * <p>This is a {@link Singleton} as AWS recommends making only one client that is used throughout
   * the application.
   */
  @Provides
  @Singleton
  @AdTechIdentityDatabaseClient
  DynamoDbEnhancedClient provideDynamoDbEnhancedClient(
      @PrivacyBudgetDatabaseClient DynamoDbClient dynamoDbClient) {
    return DynamoDbEnhancedClient.builder().dynamoDbClient(dynamoDbClient).build();
  }

  @Override
  protected Class<? extends PrivacyBudgetDatabaseBridge> getPrivacyBudgetDatabaseImplementation() {
    return AwsPrivacyBudgetDatabase.class;
  }

  @Override
  protected void configureModule() {
    bind(ObjectMapper.class)
        .annotatedWith(PrivacyBudgetObjectMapper.class)
        .to(TimeObjectMapper.class);
    bind(String.class)
        .annotatedWith(PrivacyBudgetDatabaseTableName.class)
        .toInstance(privacyBudgetDatabaseTableName());
    bind(Integer.class)
        .annotatedWith(PrivacyBudgetTtlBuffer.class)
        .toInstance(privacyBudgetTtlBuffer());
    bind(PrivacyBudgetUnitConverter.class);
    bind(AwsPrivacyBudgetConverter.class);
    bind(String.class)
        .annotatedWith(AdTechIdentityDatabaseTableName.class)
        .toInstance(adTechIdentityDatabaseTableName());
    bind(Boolean.class).annotatedWith(RequestAuthEnabled.class).toInstance(authEnabled());
  }
}
