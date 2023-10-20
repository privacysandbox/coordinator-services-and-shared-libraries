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

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.inject.Inject;
import com.google.protobuf.Timestamp;
import com.google.scp.operator.protos.shared.backend.asginstance.AsgInstanceProto.AsgInstance;
import com.google.scp.operator.protos.shared.backend.asginstance.InstanceStatusProto.InstanceStatus;
import com.google.scp.operator.shared.dao.asginstancesdb.common.AsgInstancesDao.AsgInstanceDaoException;
import com.google.scp.shared.proto.ProtoUtil;
import java.time.Instant;
import java.util.Optional;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests for SpannerAsgInstancesDao functionality. */
@RunWith(JUnit4.class)
public final class SpannerAsgInstancesDaoTest {

  @Rule public final Acai acai = new Acai(SpannerMetadataDbTestModule.class);

  @Inject SpannerAsgInstancesDao spannerAsgInstancesDao;

  String instanceName;
  AsgInstance asgInstance;

  @Before
  public void setUp() {
    instanceName =
        "https://www.googleapis.com/compute/v1/projects/test/zones/us-central1-b/instances/test-"
            + UUID.randomUUID();

    asgInstance =
        AsgInstance.newBuilder()
            .setInstanceName(instanceName)
            .setStatus(InstanceStatus.TERMINATING_WAIT)
            .setRequestTime(ProtoUtil.toProtoTimestamp(Instant.parse("2020-01-01T00:00:00Z")))
            .setTerminationTime(ProtoUtil.toProtoTimestamp(Instant.parse("2020-01-01T00:00:00Z")))
            .setTtl(ProtoUtil.toProtoTimestamp(Instant.parse("2025-01-01T00:00:00Z")).getSeconds())
            .build();
  }

  /** Test that getting a non-existent asg instance item returns an empty optional. */
  @Test
  public void getAsgInstance_getNonExistentReturnsEmpty() throws Exception {
    String nonexistentInstanceName = UUID.randomUUID().toString();

    Optional<AsgInstance> asgInstance =
        spannerAsgInstancesDao.getAsgInstance(nonexistentInstanceName);

    assertThat(asgInstance).isEmpty();
  }

  /** Test that a asg instance can be inserted and read back. */
  @Test
  public void insertThenGet() throws Exception {
    spannerAsgInstancesDao.upsertAsgInstance(asgInstance);
    Optional<AsgInstance> lookedUpAsgInstance =
        spannerAsgInstancesDao.getAsgInstance(asgInstance.getInstanceName());

    assertThat(lookedUpAsgInstance).isPresent();
    assertThat(asgInstance).isEqualTo(lookedUpAsgInstance.get());
  }

  /** Test that entry is updated when a duplicate asg instance item is inserted. */
  @Test
  public void insertJobMetadata_insertAlreadyExistsUpdates() throws Exception {
    spannerAsgInstancesDao.upsertAsgInstance(asgInstance);
    Timestamp newTimestamp = ProtoUtil.toProtoTimestamp(Instant.parse("2020-02-02T00:00:00Z"));
    AsgInstance newEntry =
        asgInstance.toBuilder()
            .setRequestTime(ProtoUtil.toProtoTimestamp(Instant.parse("2020-02-02T00:00:00Z")))
            .build();
    spannerAsgInstancesDao.upsertAsgInstance(newEntry);

    AsgInstance lookedUpAsgInstance = spannerAsgInstancesDao.getAsgInstance(instanceName).get();
    assertThat(newTimestamp).isEqualTo(lookedUpAsgInstance.getRequestTime());
  }

  /** Test inserting multiple asg instances and query for them based on status. */
  @Test
  public void getAsgInstancesByStatus_insertAndGetDifferentStatuses() throws Exception {
    spannerAsgInstancesDao.upsertAsgInstance(asgInstance);
    spannerAsgInstancesDao.upsertAsgInstance(
        asgInstance.toBuilder()
            .setInstanceName("345")
            .setStatus(InstanceStatus.TERMINATED)
            .build());
    spannerAsgInstancesDao.upsertAsgInstance(
        asgInstance.toBuilder()
            .setInstanceName("456")
            .setStatus(InstanceStatus.TERMINATED)
            .build());

    assertThat(
            spannerAsgInstancesDao
                .getAsgInstancesByStatus(InstanceStatus.TERMINATED.toString())
                .size())
        .isGreaterThan(2);
  }

  /** Test that we get an empty list when querying instances for status when there are none. */
  @Test
  public void getAsgInstancesByStatus_returnEmptyListForNoResult() throws Exception {
    assertThat(spannerAsgInstancesDao.getAsgInstancesByStatus("FakeStatus")).isEmpty();
  }

  /** Test updating an Asg Instance status. */
  @Test
  public void updateAsgInstances_updateStatus() throws Exception {
    spannerAsgInstancesDao.upsertAsgInstance(asgInstance);
    Optional<AsgInstance> lookedUpAsgInstance = spannerAsgInstancesDao.getAsgInstance(instanceName);
    assertThat(InstanceStatus.TERMINATING_WAIT).isEqualTo(lookedUpAsgInstance.get().getStatus());
    AsgInstance modifiedAsgInstance =
        lookedUpAsgInstance.get().toBuilder().setStatus(InstanceStatus.TERMINATED).build();

    spannerAsgInstancesDao.updateAsgInstance(modifiedAsgInstance);
    AsgInstance lookedUpModifiedAsgInstance =
        spannerAsgInstancesDao.getAsgInstance(instanceName).get();

    assertThat(InstanceStatus.TERMINATED).isEqualTo(lookedUpModifiedAsgInstance.getStatus());
  }

  /** Test updating a non-existent asg instance entry throws exception. */
  @Test
  public void updateAsgInstance_updateNonexistentThrows() throws Exception {
    assertThrows(
        AsgInstanceDaoException.class, () -> spannerAsgInstancesDao.updateAsgInstance(asgInstance));
  }
}
