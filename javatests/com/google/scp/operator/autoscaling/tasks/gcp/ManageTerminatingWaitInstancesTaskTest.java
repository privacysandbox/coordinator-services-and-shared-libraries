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

package com.google.scp.operator.autoscaling.tasks.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.operator.protos.shared.backend.asginstance.InstanceStatusProto.InstanceStatus.TERMINATED;
import static com.google.scp.operator.protos.shared.backend.asginstance.InstanceStatusProto.InstanceStatus.TERMINATING_WAIT;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anySet;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.acai.Acai;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.protobuf.Timestamp;
import com.google.scp.operator.protos.shared.backend.asginstance.AsgInstanceProto.AsgInstance;
import com.google.scp.operator.protos.shared.backend.asginstance.InstanceStatusProto.InstanceStatus;
import com.google.scp.operator.shared.dao.metadatadb.testing.FakeAsgInstancesDao;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class ManageTerminatingWaitInstancesTaskTest {
  @Rule public Acai acai = new Acai(ManageTerminatingWaitInstancesTaskTest.TestEnv.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private GcpInstanceManagementClient instanceManagementClient;

  @Inject FakeAsgInstancesDao fakeAsgInstancesDao;

  private ManageTerminatingWaitInstancesTask manageTerminatingWaitInstancesTask;

  private String instanceName;
  private List<String> instanceNameList;

  @Before
  public void setup() {
    instanceName =
        "https://www.googleapis.com/compute/v1/projects/test/zones/us-central1-b/instances/test-"
            + UUID.randomUUID();
    instanceNameList = new ArrayList<>();
  }

  @Test
  public void testManageInstances_nothingToDelete() throws Exception {
    when(instanceManagementClient.listActiveInstanceGroupInstances()).thenReturn(instanceNameList);
    manageTerminatingWaitInstancesTask =
        new ManageTerminatingWaitInstancesTask(fakeAsgInstancesDao, instanceManagementClient, 5);

    manageTerminatingWaitInstancesTask.manageInstances();
    verify(instanceManagementClient, never()).deleteInstances(anySet());
    assertThat(fakeAsgInstancesDao.getAsgInstancesByStatus(TERMINATING_WAIT.name())).isEmpty();
  }

  @Test
  public void testManageInstances_deleteOverdueInstances() throws Exception {
    AsgInstance asgInstance =
        AsgInstance.newBuilder()
            .setRequestTime(Timestamp.newBuilder().setSeconds(0).build())
            .setInstanceName(instanceName)
            .setStatus(InstanceStatus.TERMINATING_WAIT)
            .build();
    fakeAsgInstancesDao.setAsgInstanceToReturn(Optional.of(asgInstance));
    instanceNameList.add(instanceName);
    when(instanceManagementClient.listActiveInstanceGroupInstances()).thenReturn(instanceNameList);
    doNothing().when(instanceManagementClient).deleteInstances(anySet());
    manageTerminatingWaitInstancesTask =
        new ManageTerminatingWaitInstancesTask(fakeAsgInstancesDao, instanceManagementClient, 5);

    manageTerminatingWaitInstancesTask.manageInstances();
    verify(instanceManagementClient, times(1)).deleteInstances(anySet());
    assertEquals(TERMINATED, fakeAsgInstancesDao.getLastInstanceUpdated().getStatus());
  }

  @Test
  public void testManageInstances_instanceAlreadyDeleted() throws Exception {
    AsgInstance asgInstance =
        AsgInstance.newBuilder()
            .setRequestTime(Timestamp.newBuilder().setSeconds(0).build())
            .setInstanceName(instanceName)
            .setStatus(InstanceStatus.TERMINATING_WAIT)
            .build();
    fakeAsgInstancesDao.setAsgInstanceToReturn(Optional.of(asgInstance));
    when(instanceManagementClient.listActiveInstanceGroupInstances()).thenReturn(instanceNameList);
    manageTerminatingWaitInstancesTask =
        new ManageTerminatingWaitInstancesTask(fakeAsgInstancesDao, instanceManagementClient, 5);

    manageTerminatingWaitInstancesTask.manageInstances();
    verify(instanceManagementClient, never()).deleteInstances(anySet());
    assertEquals(TERMINATED, fakeAsgInstancesDao.getLastInstanceUpdated().getStatus());
  }

  @Test
  public void testManageInstances_nothingToDeleteActiveInstance() throws Exception {
    instanceNameList.add(instanceName);
    when(instanceManagementClient.listActiveInstanceGroupInstances()).thenReturn(instanceNameList);
    manageTerminatingWaitInstancesTask =
        new ManageTerminatingWaitInstancesTask(fakeAsgInstancesDao, instanceManagementClient, 5);

    Map<String, List<String>> remainingInstanceMap =
        manageTerminatingWaitInstancesTask.manageInstances();
    verify(instanceManagementClient, never()).deleteInstances(anySet());
    assertThat(fakeAsgInstancesDao.getAsgInstancesByStatus(TERMINATING_WAIT.name())).isEmpty();
    assertThat(remainingInstanceMap.get("us-central1-b").get(0)).isEqualTo(instanceName);
  }

  static class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      install(new FakeWorkerScaleInModule());
    }
  }
}
