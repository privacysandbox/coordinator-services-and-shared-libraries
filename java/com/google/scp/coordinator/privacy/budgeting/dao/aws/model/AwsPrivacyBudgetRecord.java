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

package com.google.scp.coordinator.privacy.budgeting.dao.aws.model;

import com.google.auto.value.AutoValue;
import com.google.scp.coordinator.privacy.budgeting.dao.aws.model.converter.InstantMillisecondsConverter;
import com.google.scp.coordinator.privacy.budgeting.dao.aws.model.converter.InstantSecondsConverter;
import java.time.Instant;
import javax.annotation.Nullable;
import software.amazon.awssdk.enhanced.dynamodb.mapper.annotations.DynamoDbAttribute;
import software.amazon.awssdk.enhanced.dynamodb.mapper.annotations.DynamoDbConvertedBy;
import software.amazon.awssdk.enhanced.dynamodb.mapper.annotations.DynamoDbImmutable;
import software.amazon.awssdk.enhanced.dynamodb.mapper.annotations.DynamoDbPartitionKey;
import software.amazon.awssdk.enhanced.dynamodb.mapper.annotations.DynamoDbSortKey;

/**
 * A model for the object that is stored in DynamoDB, the partition key is the PrivacyBudgetKey
 * (String). The sort key is the ReportingWindow (Number) that represents the timestamp in number of
 * milliseconds. The composite key (partition + sort key) uniquely identifies an item in the
 * database. The ConsumedBudgetCount (Number) is stored as an attribute that represents the consumed
 * budget count per item. The ExpiryTimestamp (Number) is a timestamp in seconds in the unix epoch
 * time format. This indicates when the item should expire and be removed from the database. It
 * takes up to 48 hours for DynamoDB to delete these items, so it can still show up in queries post
 * expiration time.
 */
@AutoValue
@AutoValue.CopyAnnotations
@DynamoDbImmutable(builder = AwsPrivacyBudgetRecord.Builder.class)
public abstract class AwsPrivacyBudgetRecord {

  public static final String PRIVACY_BUDGET_RECORD_KEY = "PrivacyBudgetRecordKey";
  public static final String REPORTING_WINDOW = "ReportingWindow";
  public static final String CONSUMED_BUDGET_COUNT = "ConsumedBudgetCount";
  public static final String EXPIRY_TIMESTAMP = "ExpiryTimestamp";

  public static AwsPrivacyBudgetRecord.Builder builder() {
    return new AutoValue_AwsPrivacyBudgetRecord.Builder();
  }

  // This consists of the attribution report to (ad-tech id) and the privacy budget key. It is
  // concatenated with an underscore (_).
  @DynamoDbPartitionKey
  @DynamoDbAttribute(PRIVACY_BUDGET_RECORD_KEY)
  public abstract String privacyBudgetRecordKey();

  @DynamoDbSortKey
  @DynamoDbAttribute(REPORTING_WINDOW)
  @DynamoDbConvertedBy(InstantMillisecondsConverter.class)
  public abstract Instant reportingWindow();

  @Nullable
  @DynamoDbAttribute(CONSUMED_BUDGET_COUNT)
  public abstract Integer consumedBudgetCount();

  // This is a timestamp in seconds in the unix epoch time format.
  @Nullable
  @DynamoDbAttribute(EXPIRY_TIMESTAMP)
  @DynamoDbConvertedBy(InstantSecondsConverter.class)
  public abstract Instant expiryTimestamp();

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder privacyBudgetRecordKey(String privacyBudgetRecordKey);

    public abstract Builder reportingWindow(Instant reportingWindow);

    public abstract Builder consumedBudgetCount(Integer consumedBudgetCount);

    public abstract Builder expiryTimestamp(Instant expiryTimestamp);

    public abstract AwsPrivacyBudgetRecord build();
  }
}
