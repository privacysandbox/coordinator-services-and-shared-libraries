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

package com.google.scp.coordinator.keymanagement.shared.dao.aws;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.DYNAMODB_ERROR;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.MISSING_KEY;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.UNKNOWN_ILLEGAL_ARGUMENT_EXCEPTION;
import static com.google.scp.shared.api.model.Code.ALREADY_EXISTS;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static java.lang.Math.min;
import static software.amazon.awssdk.enhanced.dynamodb.internal.AttributeValues.numberValue;
import static software.amazon.awssdk.enhanced.dynamodb.mapper.StaticAttributeTags.primaryPartitionKey;

import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDbUtil;
import com.google.scp.coordinator.keymanagement.shared.model.EncryptionKeyStatusUtil;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyStatusProto.EncryptionKeyStatus;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.shared.api.exception.ServiceException;
import java.time.Duration;
import java.time.Instant;
import java.util.stream.Stream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbTable;
import software.amazon.awssdk.enhanced.dynamodb.EnhancedType;
import software.amazon.awssdk.enhanced.dynamodb.Expression;
import software.amazon.awssdk.enhanced.dynamodb.Key;
import software.amazon.awssdk.enhanced.dynamodb.TableSchema;
import software.amazon.awssdk.enhanced.dynamodb.internal.converter.attribute.ListAttributeConverter;
import software.amazon.awssdk.enhanced.dynamodb.mapper.StaticImmutableTableSchema;
import software.amazon.awssdk.enhanced.dynamodb.model.BatchWriteItemEnhancedRequest;
import software.amazon.awssdk.enhanced.dynamodb.model.PutItemEnhancedRequest;
import software.amazon.awssdk.enhanced.dynamodb.model.ScanEnhancedRequest;
import software.amazon.awssdk.enhanced.dynamodb.model.WriteBatch;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;
import software.amazon.awssdk.services.dynamodb.model.ConditionalCheckFailedException;
import software.amazon.awssdk.services.dynamodb.model.DynamoDbException;

/** DynamoDB representation of {@link KeyDb} */
public final class DynamoKeyDb implements KeyDb {

  private static final int PUBLIC_KEY_SCAN_SIZE = 100;
  private static final String GREATER_THAN = " > ";
  private static final String GREATER_OR_EQUAL = " >= ";

  private static String AND(String e0, String e1) {
    return String.format("((%s) AND (%s))", e0, e1);
  }

  private static String NOT(String e) {
    return String.format(" NOT(%s) ", e);
  }

  private static final String INSTANT = ":instant";

  // Schema field names
  public static final String KEY_ID = "KeyId";
  public static final String PUBLIC_KEY = "PublicKey";
  public static final String PUBLIC_KEY_MATERIAL = "PublicKeyMaterial";
  public static final String PRIVATE_KEY = "PrivateKey";
  public static final String STATUS = "Status";
  public static final String KEY_ENCRYPTION_KEY_URI = "KeyEncryptionKeyUri";
  public static final String CREATION_TIME = "CreationTime";
  public static final String ACTIVATION_TIME = "ActivationTime";
  public static final String EXPIRATION_TIME = "ExpirationTime";
  public static final String TTL_TIME = "TtlTime";
  public static final String KEY_SPLIT_DATA = "KeySplitData";
  public static final String KEY_TYPE = "KeyType";

  private final Logger logger = LoggerFactory.getLogger(DynamoKeyDb.class);
  private final DynamoDbTable<EncryptionKey> keyTable;
  private final DynamoDbEnhancedClient ddbClient;

  @Inject
  public DynamoKeyDb(
      @KeyDbClient DynamoDbEnhancedClient ddbClient, @DynamoKeyDbTableName String tableName) {
    this.ddbClient = ddbClient;
    keyTable = ddbClient.table(tableName, getDynamoDbTableSchema());
  }

  public static TableSchema<EncryptionKey> getDynamoDbTableSchema() {
    return StaticImmutableTableSchema.builder(EncryptionKey.class, EncryptionKey.Builder.class)
        .newItemBuilder(EncryptionKey::newBuilder, EncryptionKey.Builder::build)
        .addAttribute(
            String.class,
            attribute ->
                attribute
                    .name(KEY_ID)
                    .getter(EncryptionKey::getKeyId)
                    .setter(EncryptionKey.Builder::setKeyId)
                    .tags(primaryPartitionKey()))
        .addAttribute(
            String.class,
            attribute ->
                attribute
                    .name(PUBLIC_KEY)
                    .getter(EncryptionKey::getPublicKey)
                    .setter(EncryptionKey.Builder::setPublicKey))
        .addAttribute(
            String.class,
            attribute ->
                attribute
                    .name(PUBLIC_KEY_MATERIAL)
                    .getter(EncryptionKey::getPublicKeyMaterial)
                    .setter(EncryptionKey.Builder::setPublicKeyMaterial))
        .addAttribute(
            String.class,
            attribute ->
                attribute
                    .name(PRIVATE_KEY)
                    .getter(EncryptionKey::getJsonEncodedKeyset)
                    .setter(EncryptionKey.Builder::setJsonEncodedKeyset))
        .addAttribute(
            EncryptionKeyStatus.class,
            attribute ->
                attribute
                    .name(STATUS)
                    .attributeConverter(EncryptionKeyStatusAttributeConverter.create())
                    .getter(EncryptionKey::getStatus)
                    .setter(EncryptionKey.Builder::setStatus))
        .addAttribute(
            String.class,
            attribute ->
                attribute
                    .name(KEY_ENCRYPTION_KEY_URI)
                    .getter(EncryptionKey::getKeyEncryptionKeyUri)
                    .setter(EncryptionKey.Builder::setKeyEncryptionKeyUri))
        .addAttribute(
            Long.class,
            attribute ->
                attribute
                    .name(CREATION_TIME)
                    .getter(EncryptionKey::getCreationTime)
                    .setter(EncryptionKey.Builder::setCreationTime))
        .addAttribute(
            Long.class,
            attribute ->
                attribute
                    .name(ACTIVATION_TIME)
                    .getter(EncryptionKey::getActivationTime)
                    .setter(EncryptionKey.Builder::setActivationTime))
        .addAttribute(
            Long.class,
            attribute ->
                attribute
                    .name(EXPIRATION_TIME)
                    .getter(EncryptionKey::getExpirationTime)
                    .setter(EncryptionKey.Builder::setExpirationTime))
        .addAttribute(
            Long.class,
            attribute ->
                attribute
                    .name(TTL_TIME)
                    .getter(EncryptionKey::getTtlTime)
                    .setter(EncryptionKey.Builder::setTtlTime))
        .addAttribute(
            EnhancedType.listOf(KeySplitData.class),
            attribute ->
                attribute
                    .name(KEY_SPLIT_DATA)
                    .attributeConverter(
                        ListAttributeConverter.create(KeySplitDataAttributeConverter.create()))
                    .getter(EncryptionKey::getKeySplitDataList)
                    .setter(
                        (key, keySplitData) ->
                            key.addAllKeySplitData(ImmutableList.copyOf(keySplitData))))
        .addAttribute(
            String.class,
            attribute ->
                attribute
                    .name(KEY_TYPE)
                    .getter(EncryptionKey::getKeyType)
                    .setter(EncryptionKey.Builder::setKeyType))
        .build();
  }

  @Override
  public ImmutableList<EncryptionKey> getActiveKeys(int keyLimit, Instant instant)
      throws ServiceException {
    try {
      AttributeValue instantValue = numberValue(instant.toEpochMilli());
      ImmutableList<EncryptionKey> keyList =
          keyTable
              .scan(
                  ScanEnhancedRequest.builder()
                      .filterExpression(
                          Expression.builder()
                              .expression(
                                  AND(
                                      EXPIRATION_TIME + GREATER_THAN + INSTANT,
                                      // Using NOT for backward compatibility for existing rows
                                      // without ACTIVATION_TIME.
                                      NOT(ACTIVATION_TIME + GREATER_THAN + INSTANT)))
                              .putExpressionValue(INSTANT, instantValue)
                              .build())
                      /* defines page size */
                      .limit(PUBLIC_KEY_SCAN_SIZE)
                      .build())
              .stream()
              .flatMap(page -> page.items().stream())
              .filter(EncryptionKeyStatusUtil::isActive)
              .sorted(KeyDbUtil.getActiveKeysComparator())
              .collect(toImmutableList());
      return keyList.subList(0, min(keyLimit, keyList.size()));
    } catch (DynamoDbException
        | UnsupportedOperationException
        | IllegalStateException
        | InvalidEncryptionKeyStatusException exception) {
      throw logAndReturnException(exception);
    }
  }

  /** Gets all keys from Dynamo table by performing a scan of the table. */
  @Override
  public ImmutableList<EncryptionKey> getAllKeys() throws ServiceException {
    try {
      return keyTable
          .scan(
              ScanEnhancedRequest.builder()
                  /* defines page size */
                  .limit(PUBLIC_KEY_SCAN_SIZE)
                  .build())
          .stream()
          .flatMap(page -> page.items().stream())
          .collect(toImmutableList());
    } catch (DynamoDbException
        | UnsupportedOperationException
        | IllegalStateException
        | InvalidEncryptionKeyStatusException exception) {
      throw logAndReturnException(exception);
    }
  }

  @Override
  public Stream<EncryptionKey> listRecentKeys(Duration maxAage) throws ServiceException {
    try {
      long maxCreationTime = Instant.now().minus(maxAage).toEpochMilli();
      String MAX = ":max";
      return keyTable
          .scan(
              ScanEnhancedRequest.builder()
                  .filterExpression(
                      Expression.builder()
                          .expression(CREATION_TIME + GREATER_OR_EQUAL + MAX)
                          .putExpressionValue(MAX, numberValue(maxCreationTime))
                          .build())
                  .build())
          .stream()
          .flatMap(page -> page.items().stream());
    } catch (DynamoDbException
        | UnsupportedOperationException
        | IllegalStateException
        | InvalidEncryptionKeyStatusException exception) {
      throw logAndReturnException(exception);
    }
  }
  /** Returns encryption key for specific keyId. */
  @Override
  public EncryptionKey getKey(String keyId) throws ServiceException {
    Key key = Key.builder().partitionValue(keyId).build();

    try {
      EncryptionKey record = keyTable.getItem(key);
      if (record == null) {
        throw new ServiceException(
            NOT_FOUND, MISSING_KEY.name(), "Unable to find item with keyId " + keyId);
      }

      return record;
    } catch (IllegalArgumentException e) {
      logger.error("Illegal argument exception when fetching private keys", e);
      throw new ServiceException(INTERNAL, UNKNOWN_ILLEGAL_ARGUMENT_EXCEPTION.name(), e);
    } catch (DynamoDbException e) {
      logger.error("Unknown DynamoDB error when fetching private keys", e);
      throw new ServiceException(INTERNAL, DYNAMODB_ERROR.name(), e);
    }
  }

  /**
   * Writes multiple keys to DynamoDb. This process will overwrite existing entries with the same
   * keyId.
   */
  @Override
  public void createKeys(ImmutableList<EncryptionKey> keys) throws ServiceException {
    if (keys.isEmpty()) {
      return;
    }

    var writeBatch = WriteBatch.builder(EncryptionKey.class).mappedTableResource(keyTable);
    keys.forEach(key -> writeBatch.addPutItem(r -> r.item(key)));

    try {
      ddbClient.batchWriteItem(
          BatchWriteItemEnhancedRequest.builder().writeBatches(writeBatch.build()).build());
    } catch (DynamoDbException e) {
      logger.error("Unknown DynamoDB error when saving keys", e);
      throw new ServiceException(INTERNAL, DYNAMODB_ERROR.name(), e);
    }
  }

  /** Create key with overwrite option */
  @Override
  public void createKey(EncryptionKey key, boolean overwrite) throws ServiceException {
    if (overwrite) {
      try {
        keyTable.putItem(key);
      } catch (DynamoDbException e) {
        logger.error("Unknown DynamoDB error when saving key", e);
        throw new ServiceException(INTERNAL, DYNAMODB_ERROR.name(), e);
      }
    } else {
      PutItemEnhancedRequest<EncryptionKey> putItemEnhancedRequest =
          PutItemEnhancedRequest.builder(EncryptionKey.class)
              .item(key)
              .conditionExpression(
                  Expression.builder()
                      .expression(String.format("attribute_not_exists(%s)", KEY_ID))
                      .build())
              .build();
      try {
        keyTable.putItem(putItemEnhancedRequest);
      } catch (ConditionalCheckFailedException e) {
        logger.warn(String.format("KeyId %s already exists in database", key.getKeyId()), e);
        throw new ServiceException(ALREADY_EXISTS, DYNAMODB_ERROR.name(), e);
      } catch (DynamoDbException e) {
        logger.error("Unknown DynamoDB error when saving key", e);
        throw new ServiceException(INTERNAL, DYNAMODB_ERROR.name(), e);
      }
    }
  }

  private ServiceException logAndReturnException(RuntimeException exception) {
    String message = "DynamoDB encountered error getting public keys";
    logger.error(message, exception);
    return new ServiceException(INTERNAL, DYNAMODB_ERROR.name(), message, exception);
  }
}
