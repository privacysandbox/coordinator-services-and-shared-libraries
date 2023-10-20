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

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.collect.Streams.stream;
import static com.google.scp.coordinator.privacy.budgeting.dao.aws.model.AwsPrivacyBudgetRecord.CONSUMED_BUDGET_COUNT;
import static com.google.scp.coordinator.privacy.budgeting.dao.aws.model.AwsPrivacyBudgetRecord.EXPIRY_TIMESTAMP;
import static com.google.scp.coordinator.privacy.budgeting.dao.aws.model.AwsPrivacyBudgetRecord.PRIVACY_BUDGET_RECORD_KEY;
import static com.google.scp.coordinator.privacy.budgeting.dao.aws.model.AwsPrivacyBudgetRecord.REPORTING_WINDOW;
import static software.amazon.awssdk.enhanced.dynamodb.internal.AttributeValues.numberValue;
import static software.amazon.awssdk.enhanced.dynamodb.internal.AttributeValues.stringValue;

import com.google.auto.service.AutoService;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Iterables;
import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.dao.aws.model.AwsPrivacyBudgetRecord;
import com.google.scp.coordinator.privacy.budgeting.dao.aws.model.converter.AwsPrivacyBudgetConverter;
import com.google.scp.coordinator.privacy.budgeting.dao.common.PrivacyBudgetDatabaseBridge;
import com.google.scp.coordinator.privacy.budgeting.dao.model.PrivacyBudgetRecord;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetDatabase;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetDatabaseClient;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetDatabaseTableName;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetTtlBuffer;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.ReportingWindowOffset;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbTable;
import software.amazon.awssdk.enhanced.dynamodb.Key;
import software.amazon.awssdk.enhanced.dynamodb.TableSchema;
import software.amazon.awssdk.enhanced.dynamodb.model.TransactGetItemsEnhancedRequest;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;
import software.amazon.awssdk.services.dynamodb.model.DynamoDbException;
import software.amazon.awssdk.services.dynamodb.model.ReturnValue;
import software.amazon.awssdk.services.dynamodb.model.UpdateItemRequest;
import software.amazon.awssdk.services.dynamodb.model.UpdateItemResponse;

/** AWS DynamoDb implementation of the privacy budget database. */
@AutoService(PrivacyBudgetDatabase.class)
public final class AwsPrivacyBudgetDatabase implements PrivacyBudgetDatabaseBridge {

  private static final TableSchema<AwsPrivacyBudgetRecord> PRIVACY_BUDGET_TABLE_SCHEMA =
      TableSchema.fromImmutableClass(AwsPrivacyBudgetRecord.class);
  // The max batch size for TransactGetItems API is 25.
  private static final int MAX_TRANSACT_ITEMS_BATCH_SIZE = 25;
  private static final String INCREMENT = ":inc";
  private static final String START = ":start";
  private static final String EXPIRY = ":expiry";
  // Unit to increase the privacy budget count by.
  private static final int INCREMENT_BY = 1;
  // Value of consumed privacy budget if it does not exists in the database.
  private static final int START_AT = 0;
  private static final String PRIMARY_KEY_SEPARATOR = "_";

  private static final String UPDATE_EXPRESSION_TEMPLATE =
      "SET %s = if_not_exists(%s, %s) + %s, %s = %s";

  private static final Logger logger = LoggerFactory.getLogger(AwsPrivacyBudgetDatabase.class);

  private final String tableName;
  private final DynamoDbClient ddb;
  private final DynamoDbEnhancedClient ddbEnhanced;
  private final DynamoDbTable<AwsPrivacyBudgetRecord> privacyBudgetTable;
  private final int reportingWindowOffset;
  private final int privacyBudgetTtlBuffer;
  private final AwsPrivacyBudgetConverter privacyBudgetConverter;

  @Inject
  AwsPrivacyBudgetDatabase(
      @PrivacyBudgetDatabaseClient DynamoDbClient ddb,
      @PrivacyBudgetDatabaseTableName String tableName,
      @ReportingWindowOffset int reportingWindowOffset,
      @PrivacyBudgetTtlBuffer int privacyBudgetTtlBuffer,
      AwsPrivacyBudgetConverter privacyBudgetConverter) {
    this.ddb = ddb;
    this.tableName = tableName;
    this.reportingWindowOffset = reportingWindowOffset;
    this.privacyBudgetTtlBuffer = privacyBudgetTtlBuffer;
    this.privacyBudgetConverter = privacyBudgetConverter;

    ddbEnhanced = DynamoDbEnhancedClient.builder().dynamoDbClient(ddb).build();
    privacyBudgetTable = ddbEnhanced.table(tableName, PRIVACY_BUDGET_TABLE_SCHEMA);
  }

  private static String getPartitionValue(String privacyBudgetKey, String attributionReportTo) {
    return attributionReportTo + PRIMARY_KEY_SEPARATOR + privacyBudgetKey;
  }

  private static Key buildCompositeKey(
      PrivacyBudgetUnit privacyBudgetUnit, String attributionReportTo) {
    return Key.builder()
        .partitionValue(
            getPartitionValue(privacyBudgetUnit.privacyBudgetKey(), attributionReportTo))
        .sortValue(
            privacyBudgetUnit
                .reportingWindow()
                .toEpochMilli()) // TODO(b/219774258) use string timestamp
        .build();
  }

  /**
   * First, it creates a unique set of {@link PrivacyBudgetRecord}. This needs to be a set because
   * it will eventually be used in the TransactGetItems API, which will fail if the keys we are
   * requesting is not unique. Then, it partitions the unique set of privacy budgets into lists of
   * 25, the max size of TransactGetItems API.
   *
   * @param privacyBudgetKeys privacy budget keys to query the db.
   * @return An iterable of unique and partitioned privacy budgets.
   */
  private static Iterable<List<PrivacyBudgetUnit>> getUniqueAndPartitionedPrivacyBudgets(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetKeys) {

    // ImmutableSet used to remove duplicate keys
    return Iterables.partition(
        ImmutableSet.copyOf(privacyBudgetKeys), MAX_TRANSACT_ITEMS_BATCH_SIZE);
  }

  private static ImmutableMap<String, AttributeValue> getPrimaryKeyAttributeMap(
      PrivacyBudgetUnit privacyBudgetUnit, String attributionReportTo) {
    return ImmutableMap.of(
        PRIVACY_BUDGET_RECORD_KEY,
        stringValue(getPartitionValue(privacyBudgetUnit.privacyBudgetKey(), attributionReportTo)),
        REPORTING_WINDOW,
        numberValue(
            privacyBudgetUnit
                .reportingWindow()
                .toEpochMilli())); // TODO(b/219774258) use string timestamp
  }

  /**
   * Creates an update expression where it first assigns a value to the attribute if it does not
   * exist. Then, it will increment that value. This is DynamoDB's implementation of an atomic
   * counter.
   *
   * @return The update expression to execute for incrementing the count of an attribute.
   */
  private static String getUpdateExpression() {
    return String.format(
        UPDATE_EXPRESSION_TEMPLATE,
        CONSUMED_BUDGET_COUNT,
        CONSUMED_BUDGET_COUNT,
        START,
        INCREMENT,
        EXPIRY_TIMESTAMP,
        EXPIRY);
  }

  private static PrivacyBudgetRecord mapToPrivacyBudgetRecord(
      AwsPrivacyBudgetRecord record, Map<String, String> partitionValueToPrivacyBudgetKeyMap) {
    return PrivacyBudgetRecord.builder()
        .privacyBudgetKey(partitionValueToPrivacyBudgetKeyMap.get(record.privacyBudgetRecordKey()))
        .reportingWindow(record.reportingWindow())
        .consumedBudgetCount(record.consumedBudgetCount())
        .build();
  }

  private Instant getTtl(Instant reportingWindow) {
    return reportingWindow
        .plus(reportingWindowOffset, ChronoUnit.DAYS)
        .plus(privacyBudgetTtlBuffer, ChronoUnit.DAYS);
  }

  private ImmutableMap<String, AttributeValue> getExpressionAttributeValues(
      Instant reportingWindow) {
    return ImmutableMap.of(
        INCREMENT,
        numberValue(INCREMENT_BY),
        START,
        numberValue(START_AT),
        EXPIRY,
        numberValue(getTtl(reportingWindow).getEpochSecond()));
  }

  @Override
  public ImmutableList<PrivacyBudgetRecord> getPrivacyBudgets(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits, String attributionReportTo) {
    Iterable<List<PrivacyBudgetUnit>> partitionedPrivacyBudgets =
        getUniqueAndPartitionedPrivacyBudgets(privacyBudgetUnits);

    return stream(partitionedPrivacyBudgets)
        .map(units -> getPrivacyBudgetsFromDb(units, attributionReportTo))
        .flatMap(ImmutableList::stream)
        .collect(toImmutableList());
  }

  // TODO(b/225079608) update in batches, retry on conflict
  @Override
  public ImmutableList<PrivacyBudgetRecord> incrementPrivacyBudgets(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits, String attributionReportTo) {
    return privacyBudgetUnits.stream()
        .map(
            privacyBudgetUnit ->
                this.incrementPrivacyBudget(privacyBudgetUnit, attributionReportTo))
        .filter(Optional::isPresent)
        .map(Optional::get)
        .collect(toImmutableList());
  }

  private void buildTransactGetItems(
      TransactGetItemsEnhancedRequest.Builder builder,
      List<PrivacyBudgetUnit> privacyBudgetUnits,
      String attributionReportTo) {
    privacyBudgetUnits.forEach(
        privacyBudgetUnit ->
            builder.addGetItem(
                privacyBudgetTable, buildCompositeKey(privacyBudgetUnit, attributionReportTo)));
  }

  private ImmutableList<PrivacyBudgetRecord> getPrivacyBudgetsFromDb(
      List<PrivacyBudgetUnit> privacyBudgetUnits, String attributionReportTo) {
    // Build a reverse mapping of partition values (attribution report to + privacy budget key) to
    // privacy budget key. Can't use streams here because the keys can conflict.
    Map<String, String> partitionValueToPrivacyBudgetKeyMap = new HashMap<>();
    privacyBudgetUnits.forEach(
        key ->
            partitionValueToPrivacyBudgetKeyMap.put(
                getPartitionValue(key.privacyBudgetKey(), attributionReportTo),
                key.privacyBudgetKey()));

    return ddbEnhanced
        .transactGetItems(r -> buildTransactGetItems(r, privacyBudgetUnits, attributionReportTo))
        .stream()
        .map(document -> document.getItem(privacyBudgetTable))
        .filter(Objects::nonNull)
        .map(record -> mapToPrivacyBudgetRecord(record, partitionValueToPrivacyBudgetKeyMap))
        .collect(toImmutableList());
  }

  private UpdateItemRequest getUpdateItemRequest(
      PrivacyBudgetUnit privacyBudgetUnit, String attributionReportTo) {
    return UpdateItemRequest.builder()
        .tableName(tableName)
        .key(getPrimaryKeyAttributeMap(privacyBudgetUnit, attributionReportTo))
        .updateExpression(getUpdateExpression())
        .expressionAttributeValues(
            getExpressionAttributeValues(privacyBudgetUnit.reportingWindow()))
        .returnValues(ReturnValue.UPDATED_NEW)
        .build();
  }

  /**
   * Returns the updated privacy budget after incrementing the budget by 1 for a given key.
   *
   * @param privacyBudgetUnit The privacy-budget-unit to increment.
   * @param attributionReportTo The ad-tech id.
   * @return The updated privacy budget. If any errors occurred when updating the database, nothing
   *     is returned.
   */
  private Optional<PrivacyBudgetRecord> incrementPrivacyBudget(
      PrivacyBudgetUnit privacyBudgetUnit, String attributionReportTo) {
    try {
      UpdateItemResponse response =
          ddb.updateItem(getUpdateItemRequest(privacyBudgetUnit, attributionReportTo));
      int consumedBudgetCount =
          Integer.parseInt(response.attributes().get(CONSUMED_BUDGET_COUNT).n());

      return Optional.of(
          PrivacyBudgetRecord.builder()
              .privacyBudgetKey(privacyBudgetUnit.privacyBudgetKey())
              .reportingWindow(privacyBudgetUnit.reportingWindow())
              .consumedBudgetCount(consumedBudgetCount)
              .build());
    } catch (DynamoDbException e) {
      logger.warn(
          String.format(
              "Failed to increment privacy budget for unit: %s in DynamoDB", privacyBudgetUnit),
          e);
      return Optional.empty();
    }
  }
}
