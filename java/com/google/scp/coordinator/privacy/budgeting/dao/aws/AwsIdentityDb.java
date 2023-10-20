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

package com.google.scp.coordinator.privacy.budgeting.dao.aws;

import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.model.AdTechIdentity;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.AdTechIdentityDatabaseClient;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.AdTechIdentityDatabaseTableName;
import java.util.Optional;
import software.amazon.awssdk.core.exception.SdkException;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbTable;
import software.amazon.awssdk.enhanced.dynamodb.Key;
import software.amazon.awssdk.enhanced.dynamodb.TableSchema;
import software.amazon.awssdk.enhanced.dynamodb.mapper.StaticAttributeTags;
import software.amazon.awssdk.enhanced.dynamodb.mapper.StaticImmutableTableSchema;

/**
 * Retrieves identity information for handling requests.
 *
 * <p>TODO(b/233384982) Add a IdentityDb interface that this implements
 */
public final class AwsIdentityDb {

  private final DynamoDbTable<AdTechIdentity> identityTable;

  @Inject
  AwsIdentityDb(
      @AdTechIdentityDatabaseClient DynamoDbEnhancedClient ddbClient,
      @AdTechIdentityDatabaseTableName String tableName) {
    identityTable = ddbClient.table(tableName, tableSchema());
  }

  /**
   * Retrieves the identity of the ad-tech from the DB
   *
   * @param iamRole the IAM role of the requester to look up with
   * @return {@link Optional} containing the identity if a value was present. Otherwise, empty.
   * @throws AwsIdentityDbException if an error occurs
   */
  public Optional<AdTechIdentity> getIdentity(String iamRole) throws AwsIdentityDbException {
    Key key = Key.builder().partitionValue(iamRole).build();
    try {
      return Optional.ofNullable(identityTable.getItem(key));
    } catch (SdkException e) {
      throw new AwsIdentityDbException(e);
    }
  }

  private static TableSchema<AdTechIdentity> tableSchema() {
    return StaticImmutableTableSchema.builder(AdTechIdentity.class, AdTechIdentity.Builder.class)
        .newItemBuilder(AdTechIdentity::builder, AdTechIdentity.Builder::build)
        .addAttribute(
            String.class,
            attribute ->
                attribute
                    .name("IamRole")
                    .getter(AdTechIdentity::iamRole)
                    .setter(AdTechIdentity.Builder::setIamRole)
                    .tags(StaticAttributeTags.primaryPartitionKey()))
        .addAttribute(
            String.class,
            attribute ->
                attribute
                    .name("ReportingOrigin")
                    .getter(AdTechIdentity::attributionReportTo)
                    .setter(AdTechIdentity.Builder::setAttributionReportTo))
        .build();
  }

  /** Exception for errors that originate in the {@link AwsIdentityDb} */
  public static class AwsIdentityDbException extends Exception {
    public AwsIdentityDbException(Throwable cause) {
      super(cause);
    }
  }
}
