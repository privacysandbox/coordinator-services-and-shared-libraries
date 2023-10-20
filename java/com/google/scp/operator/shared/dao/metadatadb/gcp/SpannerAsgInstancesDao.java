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

package com.google.scp.operator.shared.dao.metadatadb.gcp;

import static com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerAsgInstancesDao.SpannerAsgInstancesTableColumn.INSTANCE_NAME;
import static com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerAsgInstancesDao.SpannerAsgInstancesTableColumn.REQUEST_TIME;
import static com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerAsgInstancesDao.SpannerAsgInstancesTableColumn.STATUS;
import static com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerAsgInstancesDao.SpannerAsgInstancesTableColumn.TERMINATION_TIME;
import static com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerAsgInstancesDao.SpannerAsgInstancesTableColumn.TTL;
import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.Mutation;
import com.google.cloud.spanner.ResultSet;
import com.google.cloud.spanner.SpannerException;
import com.google.cloud.spanner.Statement;
import com.google.common.collect.ImmutableList;
import com.google.inject.BindingAnnotation;
import com.google.inject.Inject;
import com.google.protobuf.Timestamp;
import com.google.scp.operator.protos.shared.backend.asginstance.AsgInstanceProto.AsgInstance;
import com.google.scp.operator.protos.shared.backend.asginstance.InstanceStatusProto.InstanceStatus;
import com.google.scp.operator.shared.dao.asginstancesdb.common.AsgInstancesDao;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobMetadataDbClient;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.TimeUnit;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Spanner implementation of the {@code AsgInstancesDao} */
public class SpannerAsgInstancesDao implements AsgInstancesDao {
  private static final Logger logger = LoggerFactory.getLogger(SpannerAsgInstancesDao.class);

  /** Table name for the autoscaling group instances DB. */
  public static final String TABLE_NAME = "AsgInstances";

  private final DatabaseClient dbClient;

  /** Creates a new instance of the {@code SpannerAsgInstancesDao} class. */
  @Inject
  SpannerAsgInstancesDao(@JobMetadataDbClient DatabaseClient dbClient) {
    this.dbClient = dbClient;
  }

  @Override
  public Optional<AsgInstance> getAsgInstance(String instanceName) throws AsgInstanceDaoException {

    Statement statement =
        Statement.newBuilder(
                "SELECT * FROM "
                    + TABLE_NAME
                    + " WHERE "
                    + INSTANCE_NAME.label
                    + " = @instanceName")
            .bind("instanceName")
            .to(instanceName)
            .build();
    try (ResultSet resultSet = dbClient.singleUse().executeQuery(statement)) {
      // Expecting only one row to be returned since searching on primary key
      return resultSet.next()
          ? Optional.of(convertResultSetToAsgInstance(resultSet))
          : Optional.empty();
    } catch (SpannerException e) {
      throw new AsgInstanceDaoException(e);
    }
  }

  @Override
  public List<AsgInstance> getAsgInstancesByStatus(String status) throws AsgInstanceDaoException {
    List<AsgInstance> asgInstances = new ArrayList();

    Statement statement =
        Statement.newBuilder(
                "SELECT * FROM " + TABLE_NAME + " WHERE " + STATUS.label + " = @status")
            .bind("status")
            .to(status)
            .build();
    try (ResultSet resultSet = dbClient.singleUse().executeQuery(statement)) {
      while (resultSet.next()) {
        asgInstances.add(convertResultSetToAsgInstance(resultSet));
      }
      return asgInstances;
    } catch (SpannerException e) {
      throw new AsgInstanceDaoException(e);
    }
  }

  @Override
  public void upsertAsgInstance(AsgInstance asgInstance) throws AsgInstanceDaoException {
    try {
      com.google.cloud.Timestamp ttl =
          com.google.cloud.Timestamp.ofTimeSecondsAndNanos(
              TimeUnit.SECONDS.convert(asgInstance.getTtl(), TimeUnit.SECONDS), 0);

      ImmutableList<Mutation> inserts =
          ImmutableList.of(
              Mutation.newInsertOrUpdateBuilder(TABLE_NAME)
                  .set(INSTANCE_NAME.label)
                  .to(asgInstance.getInstanceName())
                  .set(STATUS.label)
                  .to(asgInstance.getStatus().toString())
                  .set(REQUEST_TIME.label)
                  .to(com.google.cloud.Timestamp.fromProto(asgInstance.getRequestTime()))
                  .set(TERMINATION_TIME.label)
                  .to(
                      asgInstance.hasTerminationTime()
                          ? com.google.cloud.Timestamp.fromProto(asgInstance.getTerminationTime())
                          : null)
                  .set(TTL.label)
                  .to(ttl)
                  .build());
      dbClient.write(inserts);
      logger.info(
          String.format(
              "Wrote instance '%s' to autoscaling group instances db.",
              asgInstance.getInstanceName()));
    } catch (SpannerException e) {
      throw new AsgInstanceDaoException(e);
    }
  }

  /**
   * Updates an AsgInstance record. Spanner API will throw an exception if the instance record does
   * not exist.
   */
  @Override
  public void updateAsgInstance(AsgInstance asgInstance) throws AsgInstanceDaoException {
    try {
      dbClient
          .readWriteTransaction()
          .run(
              transaction -> {
                ImmutableList<Mutation> updates =
                    ImmutableList.of(
                        Mutation.newUpdateBuilder(TABLE_NAME)
                            .set(INSTANCE_NAME.label)
                            .to(asgInstance.getInstanceName())
                            .set(STATUS.label)
                            .to(asgInstance.getStatus().toString())
                            .set(REQUEST_TIME.label)
                            .to(com.google.cloud.Timestamp.fromProto(asgInstance.getRequestTime()))
                            .set(TERMINATION_TIME.label)
                            .to(
                                asgInstance.hasTerminationTime()
                                    ? com.google.cloud.Timestamp.fromProto(
                                        asgInstance.getTerminationTime())
                                    : null)
                            .build());
                logger.debug("Buffering spanner updates: " + updates);
                transaction.buffer(updates);
                logger.info(
                    String.format(
                        "Updated instance '%s' in spanner autoscaling group instances db.",
                        asgInstance.getInstanceName()));
                return null;
              });
    } catch (SpannerException e) {
      throw new AsgInstanceDaoException(e);
    }
  }

  private AsgInstance convertResultSetToAsgInstance(ResultSet resultSet) {
    String computeInstanceId = resultSet.getString(INSTANCE_NAME.label);
    String status = resultSet.getString(STATUS.label);
    Timestamp requestTime = resultSet.getTimestamp(REQUEST_TIME.label).toProto();
    long ttl = resultSet.getTimestamp(TTL.label).getSeconds();

    AsgInstance.Builder asgInstanceBuilder =
        AsgInstance.newBuilder()
            .setInstanceName(computeInstanceId)
            .setStatus(InstanceStatus.valueOf(status))
            .setRequestTime(requestTime)
            .setTtl(ttl);

    if (!resultSet.isNull(TERMINATION_TIME.label)) {
      asgInstanceBuilder.setTerminationTime(
          resultSet.getTimestamp(TERMINATION_TIME.label).toProto());
    }
    return asgInstanceBuilder.build();
  }

  /** Column names for the Spanner autoscaling group instances table. */
  enum SpannerAsgInstancesTableColumn {
    INSTANCE_NAME("InstanceName"),
    STATUS("Status"),
    REQUEST_TIME("RequestTime"),
    TERMINATION_TIME("TerminationTime"),
    TTL("Ttl");

    /** Value of a {@code SpannerAsgInstancesTableColumn} constant. */
    public final String label;

    /** Constructor for setting {@code SpannerAsgInstancesTableColumn} enum constants. */
    SpannerAsgInstancesTableColumn(String label) {
      this.label = label;
    }
  }

  /** Annotation for the int of the TTL days to use for the AsgInstances DB. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface AsgInstancesDbSpannerTtlDays {}
}
