package com.google.scp.coordinator.keymanagement.shared.dao.gcp;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.DATASTORE_ERROR;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.MISSING_KEY;
import static com.google.scp.shared.api.model.Code.ALREADY_EXISTS;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;

import com.google.cloud.Timestamp;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.ErrorCode;
import com.google.cloud.spanner.Mutation;
import com.google.cloud.spanner.ResultSet;
import com.google.cloud.spanner.SpannerException;
import com.google.cloud.spanner.Statement;
import com.google.cloud.spanner.Value;
import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitDataList;
import com.google.scp.shared.api.exception.ServiceException;
import java.time.Duration;
import java.time.Instant;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.stream.Stream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** KeyDb implementation for GCP Spanner. */
public final class SpannerKeyDb implements KeyDb {

  private static final String KEY_ID_COLUMN = "KeyId";
  private static final String PUBLIC_KEY_COLUMN = "PublicKey";
  private static final String PRIVATE_KEY_COLUMN = "PrivateKey";
  private static final String PUBLIC_KEY_MATERIAL_COLUMN = "PublicKeyMaterial";
  private static final String KEY_SPLIT_DATA_COLUMN = "KeySplitData";
  private static final String KEY_TYPE = "KeyType";
  private static final String KEY_ENCRYPTION_KEY_URI = "KeyEncryptionKeyUri";
  private static final String EXPIRY_TIME_COLUMN = "ExpiryTime";
  private static final String TTL_TIME_COLUMN = "TtlTime";
  private static final String ACTIVATION_TIME_COLUMN = "ActivationTime";
  private static final String CREATED_AT_COLUMN = "CreatedAt";
  private static final String UPDATED_AT_COLUMN = "UpdatedAt";
  private static final String TABLE_NAME = "KeySets";
  private static final int EXPIRATION_TIME_CEILING_SECOND = 1;
  private static final JsonFormat.Printer JSON_PRINTER = JsonFormat.printer();
  private static final JsonFormat.Parser JSON_PARSER = JsonFormat.parser();
  private static final Logger LOGGER = LoggerFactory.getLogger(SpannerKeyDb.class);

  private final DatabaseClient dbClient;
  private final Integer readStalenessSeconds;

  @Inject
  public SpannerKeyDb(@KeyDbClient DatabaseClient dbClient, SpannerKeyDbConfig config) {
    this.dbClient = dbClient;
    this.readStalenessSeconds = config.readStalenessSeconds();
  }

  @Override
  public ImmutableList<EncryptionKey> getActiveKeys(int keyLimit, Instant instant)
      throws ServiceException {
    Statement statement =
        Statement.newBuilder(
                "SELECT * FROM "
                    + TABLE_NAME
                    + " WHERE "
                    + EXPIRY_TIME_COLUMN
                    + " > @nowParam AND ("
                    + ACTIVATION_TIME_COLUMN
                    + " is NULL OR "
                    + ACTIVATION_TIME_COLUMN
                    + " <= @nowParam)"
                    // Ordering implementation should follow {@link
                    // KeyDbUtil.getActiveKeysComparator}
                    + " ORDER BY "
                    + EXPIRY_TIME_COLUMN
                    + " DESC, "
                    + ACTIVATION_TIME_COLUMN
                    + " DESC LIMIT @keyLimitParam")
            .bind("nowParam")
            .to(Timestamp.ofTimeSecondsAndNanos(instant.getEpochSecond(), instant.getNano()))
            .bind("keyLimitParam")
            .to(keyLimit)
            .build();
    ImmutableList.Builder<EncryptionKey> keysBuilder = ImmutableList.builder();
    try (ResultSet resultSet = dbClient.singleUse().executeQuery(statement)) {
      while (resultSet.next()) {
        keysBuilder.add(buildEncryptionKey(resultSet));
      }
    }
    return keysBuilder.build();
  }

  @Override
  public ImmutableList<EncryptionKey> getAllKeys() throws ServiceException {
    Statement statement = Statement.newBuilder("SELECT * FROM " + TABLE_NAME).build();
    ImmutableList.Builder<EncryptionKey> keysBuilder = ImmutableList.builder();
    try (ResultSet resultSet = dbClient.singleUse().executeQuery(statement)) {
      while (resultSet.next()) {
        keysBuilder.add(buildEncryptionKey(resultSet));
      }
    }
    return keysBuilder.build();
  }

  @Override
  public Stream<EncryptionKey> listRecentKeys(Duration maxAge) throws ServiceException {
    Instant maxCreation = Instant.now().minus(maxAge);
    Statement statement =
        Statement.newBuilder(
                "SELECT * FROM "
                    + TABLE_NAME
                    + " WHERE "
                    + CREATED_AT_COLUMN
                    + " >= @nowParam "
                    + " ORDER BY "
                    + EXPIRY_TIME_COLUMN
                    + " DESC, "
                    + ACTIVATION_TIME_COLUMN
                    + " DESC")
            .bind("nowParam")
            .to(
                Timestamp.ofTimeSecondsAndNanos(
                    maxCreation.getEpochSecond(), maxCreation.getNano()))
            .build();
    Stream.Builder<EncryptionKey> keysBuilder = Stream.builder();
    try (ResultSet resultSet = dbClient.singleUse().executeQuery(statement)) {
      while (resultSet.next()) {
        keysBuilder.add(buildEncryptionKey(resultSet));
      }
    }
    return keysBuilder.build();
  }

  @Override
  public EncryptionKey getKey(String keyId) throws ServiceException {
    Statement statement =
        Statement.newBuilder("SELECT * FROM " + TABLE_NAME + " WHERE KeyId = @KeyIdParam")
            .bind("KeyIdParam")
            .to(keyId)
            .build();
    ImmutableList.Builder<EncryptionKey> keysBuilder = ImmutableList.builder();
    try (ResultSet resultSet = dbClient.singleUse().executeQuery(statement)) {
      while (resultSet.next()) {
        keysBuilder.add(buildEncryptionKey(resultSet));
      }
    }
    ImmutableList<EncryptionKey> keys = keysBuilder.build();
    if (keys.size() == 0) {
      throw new ServiceException(
          NOT_FOUND, MISSING_KEY.name(), "Unable to find item with keyId " + keyId);
    } else if (keys.size() > 1) {
      throw new ServiceException(
          INTERNAL, DATASTORE_ERROR.name(), "Multiple keys found with keyId " + keyId);
    }
    return keys.get(0);
  }

  @Override
  public void createKey(EncryptionKey key, boolean overwrite) throws ServiceException {
    List<Mutation> mutations;
    mutations =
        ImmutableList.of(key).stream()
            .map(a -> toMutation(a, overwrite))
            .collect(toImmutableList());
    writeTransaction(mutations);
  }

  @Override
  public void createKeys(ImmutableList<EncryptionKey> keys) throws ServiceException {
    List<Mutation> mutations =
        keys.stream().map(a -> toMutation(a, true)).collect(toImmutableList());
    writeTransaction(mutations);
  }

  private void writeTransaction(List<Mutation> mutations) throws ServiceException {
    try {
      dbClient
          .readWriteTransaction()
          .run(
              transaction -> {
                transaction.buffer(mutations);
                return null;
              });
    } catch (SpannerException ex) {
      if (ex.getErrorCode().equals(ErrorCode.ALREADY_EXISTS)) {
        String message = "KeyId already exists in database";
        LOGGER.warn(message);
        throw new ServiceException(ALREADY_EXISTS, DATASTORE_ERROR.name(), message, ex);
      }
      String message = "Spanner encountered error creating keys";
      LOGGER.error(message, ex);
      throw new ServiceException(INTERNAL, DATASTORE_ERROR.name(), message, ex);
    }
  }

  private static Mutation toMutation(EncryptionKey key, boolean overwrite) {
    Timestamp expireTime =
        Timestamp.ofTimeMicroseconds(
            TimeUnit.MICROSECONDS.convert(key.getExpirationTime(), TimeUnit.MILLISECONDS));
    // TTL is saved in seconds, however Spanner requires a timestamp
    Timestamp ttlTime =
        Timestamp.ofTimeSecondsAndNanos(
            TimeUnit.SECONDS.convert(key.getTtlTime(), TimeUnit.SECONDS), 0);
    Timestamp activationTime =
        Timestamp.ofTimeMicroseconds(
            TimeUnit.MICROSECONDS.convert(key.getActivationTime(), TimeUnit.MILLISECONDS));
    // Serialize keySplitData to JSON and wrap it in Value object for Spanner
    Value keySplitJsonValue;
    try {
      KeySplitDataList proto =
          KeySplitDataList.newBuilder().addAllKeySplitData(key.getKeySplitDataList()).build();
      String json = JSON_PRINTER.print(proto);
      keySplitJsonValue = Value.json(json);
    } catch (InvalidProtocolBufferException ex) {
      // TODO: remove after proto migration. Currently cannot throw checked exception in java
      // stream.
      String message = "Spanner encountered keySplitData serialization error";
      LOGGER.error(message, ex);
      throw new RuntimeException(message, ex);
    }
    Mutation.WriteBuilder builder = Mutation.newInsertBuilder(SpannerKeyDb.TABLE_NAME);
    if (overwrite) {
      builder = Mutation.newInsertOrUpdateBuilder(SpannerKeyDb.TABLE_NAME);
    }

    return builder
        .set(KEY_ID_COLUMN)
        .to(key.getKeyId())
        .set(PUBLIC_KEY_COLUMN)
        .to(key.getPublicKey())
        .set(PUBLIC_KEY_MATERIAL_COLUMN)
        .to(key.getPublicKeyMaterial())
        .set(PRIVATE_KEY_COLUMN)
        .to(key.getJsonEncodedKeyset())
        .set(KEY_SPLIT_DATA_COLUMN)
        .to(keySplitJsonValue)
        .set(KEY_TYPE)
        .to(key.getKeyType())
        .set(KEY_ENCRYPTION_KEY_URI)
        .to(key.getKeyEncryptionKeyUri())
        .set(EXPIRY_TIME_COLUMN)
        .to(expireTime)
        .set(TTL_TIME_COLUMN)
        .to(ttlTime)
        .set(ACTIVATION_TIME_COLUMN)
        .to(activationTime)
        .set(CREATED_AT_COLUMN)
        .to(Value.COMMIT_TIMESTAMP)
        .set(UPDATED_AT_COLUMN)
        .to(Value.COMMIT_TIMESTAMP)
        .build();
  }

  private static EncryptionKey buildEncryptionKey(ResultSet resultSet) throws ServiceException {
    List<KeySplitData> keySplitData;
    try {
      String json = resultSet.getJson(KEY_SPLIT_DATA_COLUMN);
      KeySplitDataList.Builder builder = KeySplitDataList.newBuilder();
      JSON_PARSER.merge(json, builder);
      keySplitData = builder.build().getKeySplitDataList();
    } catch (InvalidProtocolBufferException ex) {
      String message = "Spanner encountered deserialization error with KeySplitData";
      LOGGER.error(message, ex);
      throw new ServiceException(INTERNAL, DATASTORE_ERROR.name(), message, ex);
    }
    return EncryptionKey.newBuilder()
        .setKeyId(resultSet.getString(KEY_ID_COLUMN))
        .setPublicKey(resultSet.getString(PUBLIC_KEY_COLUMN))
        .setPublicKeyMaterial(resultSet.getString(PUBLIC_KEY_MATERIAL_COLUMN))
        .setJsonEncodedKeyset(resultSet.getString(PRIVATE_KEY_COLUMN))
        .addAllKeySplitData(keySplitData)
        .setKeyType(resultSet.getString(KEY_TYPE))
        .setKeyEncryptionKeyUri(resultSet.getString(KEY_ENCRYPTION_KEY_URI))
        .setCreationTime(toEpochMilliSeconds(resultSet.getTimestamp(CREATED_AT_COLUMN)))
        .setExpirationTime(toEpochMilliSeconds(resultSet.getTimestamp(EXPIRY_TIME_COLUMN)))
        .setTtlTime(resultSet.getTimestamp(TTL_TIME_COLUMN).getSeconds())
        .setActivationTime(toEpochMilliSeconds(resultSet.getTimestamp(ACTIVATION_TIME_COLUMN)))
        .build();
  }

  private static long toEpochMilliSeconds(Timestamp timestamp) throws ServiceException {
    Instant instant = Instant.ofEpochSecond(timestamp.getSeconds(), timestamp.getNanos());
    return instant.toEpochMilli();
  }
}
