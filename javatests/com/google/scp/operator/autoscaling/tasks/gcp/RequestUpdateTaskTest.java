/*
 * Copyright 2023 Google LLC
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

import static com.google.scp.operator.protos.shared.backend.asginstance.InstanceStatusProto.InstanceStatus.TERMINATING_WAIT;
import static com.google.scp.operator.protos.shared.backend.asginstance.InstanceTerminationReasonProto.InstanceTerminationReason.UPDATE;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import com.google.acai.Acai;
import com.google.cloud.compute.v1.Autoscaler;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Singleton;
import com.google.scp.operator.shared.dao.metadatadb.testing.FakeAsgInstancesDao;
import com.google.scp.operator.shared.testing.FakeClock;
import java.time.Clock;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
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
public class RequestUpdateTaskTest {
  @Rule public Acai acai = new Acai(RequestUpdateTaskTest.TestEnv.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private GcpInstanceManagementClient instanceManagementClient;

  @Inject FakeAsgInstancesDao fakeAsgInstancesDao;
  @Inject Clock clock;
  private final int ttlDays = 365;

  private RequestUpdateTask requestUpdateTask;

  private String instanceTemplate;
  private String instanceZone;
  private GcpComputeInstance instance;
  private HashMap<String, List<GcpComputeInstance>> remainingInstances;

  @Before
  public void setup() {
    String instanceName =
        "https://www.googleapis.com/compute/v1/projects/test/zones/us-central1-b/instances/test-"
            + UUID.randomUUID();
    instanceTemplate = "test-template123456789";
    instanceZone = "us-central1-b";
    instance =
        GcpComputeInstance.builder()
            .setInstanceId(instanceName)
            .setInstanceTemplate(instanceTemplate)
            .build();
    remainingInstances = new HashMap<>();
  }

  @Test
  public void testRequestUpdate_templateMismatch() {
    Autoscaler autoscaler = Autoscaler.newBuilder().setRecommendedSize(0).build();
    when(instanceManagementClient.getAutoscaler()).thenReturn(Optional.of(autoscaler));
    requestUpdateTask =
        new RequestUpdateTask(instanceManagementClient, fakeAsgInstancesDao, clock, ttlDays);
    remainingInstances.put(instanceZone, Collections.singletonList(instance));

    when(instanceManagementClient.getCurrentInstanceTemplate()).thenReturn("non-matching-template");
    requestUpdateTask.requestUpdate(remainingInstances);

    assertEquals(TERMINATING_WAIT, fakeAsgInstancesDao.getLastInstanceInserted().getStatus());
    assertEquals(UPDATE, fakeAsgInstancesDao.getLastInstanceInserted().getTerminationReason());
    assertEquals(
        instance.getInstanceId(), fakeAsgInstancesDao.getLastInstanceInserted().getInstanceName());
  }

  @Test
  public void testRequestUpdate_templateMatch() {
    Autoscaler autoscaler = Autoscaler.newBuilder().setRecommendedSize(0).build();
    when(instanceManagementClient.getAutoscaler()).thenReturn(Optional.of(autoscaler));
    requestUpdateTask =
        new RequestUpdateTask(instanceManagementClient, fakeAsgInstancesDao, clock, ttlDays);
    remainingInstances.put(instanceZone, Collections.singletonList(instance));

    when(instanceManagementClient.getCurrentInstanceTemplate())
        .thenReturn("test-template123456789");
    requestUpdateTask.requestUpdate(remainingInstances);

    assertNull(fakeAsgInstancesDao.getLastInstanceInserted());
  }

  @Test
  public void testRequestUpdate_multipleInstances() {
    Autoscaler autoscaler = Autoscaler.newBuilder().setRecommendedSize(0).build();
    when(instanceManagementClient.getAutoscaler()).thenReturn(Optional.of(autoscaler));
    requestUpdateTask =
        new RequestUpdateTask(instanceManagementClient, fakeAsgInstancesDao, clock, ttlDays);

    String zoneA = "us-central1-a";
    String zoneB = "us-central1-b";
    remainingInstances.put(
        zoneA,
        Arrays.asList(
            createZonalGcpComputeInstance(zoneA, instanceTemplate),
            createZonalGcpComputeInstance(zoneA, instanceTemplate),
            createZonalGcpComputeInstance(zoneA, "non-matching-template")));
    // 1 instances in zoneB (us-central1-b)
    remainingInstances.put(
        zoneB,
        Collections.singletonList(createZonalGcpComputeInstance(zoneB, "non-matching-template")));

    when(instanceManagementClient.getCurrentInstanceTemplate())
        .thenReturn("test-template123456789");
    Map<String, List<GcpComputeInstance>> remainingZonesToInstances =
        requestUpdateTask.requestUpdate(remainingInstances);

    assertTrue(remainingZonesToInstances.containsKey(zoneA));
    assertFalse(remainingZonesToInstances.containsKey(zoneB));
    assertEquals(remainingZonesToInstances.get(zoneA).size(), 2);
  }

  static class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      install(new FakeWorkerScaleInModule());
      bind(Clock.class).to(FakeClock.class).in(Singleton.class);
    }
  }

  private String createZonalInstanceUrl(String instanceZone) {
    return String.format(
        "https://www.googleapis.com/compute/v1/projects/test/zones/%s/instances/test-%s",
        instanceZone, UUID.randomUUID());
  }

  private GcpComputeInstance createZonalGcpComputeInstance(String zone, String template) {
    return GcpComputeInstance.builder()
        .setInstanceId(createZonalInstanceUrl(zone))
        .setInstanceTemplate(template)
        .build();
  }
}
