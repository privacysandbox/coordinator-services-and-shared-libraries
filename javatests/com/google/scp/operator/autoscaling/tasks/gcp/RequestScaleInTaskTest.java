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

import static com.google.scp.operator.protos.shared.backend.asginstance.InstanceStatusProto.InstanceStatus.TERMINATING_WAIT;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
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
public final class RequestScaleInTaskTest {
  @Rule public Acai acai = new Acai(RequestScaleInTaskTest.TestEnv.class);
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private GcpInstanceManagementClient instanceManagementClient;

  @Inject FakeAsgInstancesDao fakeAsgInstancesDao;
  @Inject Clock clock;
  private final int ttlDays = 365;

  private RequestScaleInTask requestScaleInTask;

  private String instanceName;
  private String instanceZone;
  private HashMap<String, List<String>> remainingInstances;

  @Before
  public void setup() {
    instanceName =
        "https://www.googleapis.com/compute/v1/projects/test/zones/us-central1-b/instances/test-"
            + UUID.randomUUID();
    instanceZone = "us-central1-b";
    remainingInstances = new HashMap<>();
  }

  @Test
  public void testRequestScaleIn_greaterRecommendedSize() {
    Autoscaler autoscaler = Autoscaler.newBuilder().setRecommendedSize(2).build();
    when(instanceManagementClient.getAutoscaler()).thenReturn(Optional.of(autoscaler));
    requestScaleInTask =
        new RequestScaleInTask(instanceManagementClient, fakeAsgInstancesDao, clock, ttlDays);

    requestScaleInTask.requestScaleIn(remainingInstances);

    assertNull(fakeAsgInstancesDao.getLastInstanceInserted());
  }

  @Test
  public void testRequestScaleIn_equalRecommendedSize() {
    Autoscaler autoscaler = Autoscaler.newBuilder().setRecommendedSize(0).build();
    when(instanceManagementClient.getAutoscaler()).thenReturn(Optional.of(autoscaler));
    requestScaleInTask =
        new RequestScaleInTask(instanceManagementClient, fakeAsgInstancesDao, clock, ttlDays);

    requestScaleInTask.requestScaleIn(remainingInstances);

    assertNull(fakeAsgInstancesDao.getLastInstanceInserted());
  }

  @Test
  public void testRequestScaleIn_lessRecommendedSize() {
    Autoscaler autoscaler = Autoscaler.newBuilder().setRecommendedSize(0).build();
    when(instanceManagementClient.getAutoscaler()).thenReturn(Optional.of(autoscaler));
    requestScaleInTask =
        new RequestScaleInTask(instanceManagementClient, fakeAsgInstancesDao, clock, ttlDays);
    remainingInstances.put(instanceZone, Arrays.asList(instanceName));

    requestScaleInTask.requestScaleIn(remainingInstances);

    assertEquals(TERMINATING_WAIT, fakeAsgInstancesDao.getLastInstanceInserted().getStatus());
    assertEquals(instanceName, fakeAsgInstancesDao.getLastInstanceInserted().getInstanceName());
  }

  @Test
  public void testRequestScaleIn_balanceEvenlyByZone() {
    Autoscaler autoscaler = Autoscaler.newBuilder().setRecommendedSize(2).build();
    when(instanceManagementClient.getAutoscaler()).thenReturn(Optional.of(autoscaler));
    requestScaleInTask =
        new RequestScaleInTask(instanceManagementClient, fakeAsgInstancesDao, clock, ttlDays);
    // 2 instances in us-central1-a
    remainingInstances.put(
        "us-central1-a",
        Arrays.asList(
            createZonalInstanceUrl("us-central1-a"), createZonalInstanceUrl("us-central1-a")));
    // 2 instances in us-central1-b
    remainingInstances.put(
        "us-central1-b",
        Arrays.asList(
            createZonalInstanceUrl("us-central1-b"), createZonalInstanceUrl("us-central1-b")));

    requestScaleInTask.requestScaleIn(remainingInstances);
    Map<String, Integer> terminationZoneToInstance =
        fakeAsgInstancesDao.getZoneToInstanceCountMap();

    assertEquals(terminationZoneToInstance.get("us-central1-a"), Integer.valueOf(1));
    assertEquals(terminationZoneToInstance.get("us-central1-b"), Integer.valueOf(1));
  }

  @Test
  public void testRequestScaleIn_overloadedZone() {
    Autoscaler autoscaler = Autoscaler.newBuilder().setRecommendedSize(2).build();
    when(instanceManagementClient.getAutoscaler()).thenReturn(Optional.of(autoscaler));
    requestScaleInTask =
        new RequestScaleInTask(instanceManagementClient, fakeAsgInstancesDao, clock, ttlDays);
    // 3 instances in us-central1-a
    remainingInstances.put(
        "us-central1-a",
        Arrays.asList(
            createZonalInstanceUrl("us-central1-a"),
            createZonalInstanceUrl("us-central1-a"),
            createZonalInstanceUrl("us-central1-a")));
    // 1 instances in us-central1-b
    remainingInstances.put("us-central1-b", Arrays.asList(createZonalInstanceUrl("us-central1-b")));

    requestScaleInTask.requestScaleIn(remainingInstances);
    Map<String, Integer> terminationZoneToInstance =
        fakeAsgInstancesDao.getZoneToInstanceCountMap();

    assertEquals(terminationZoneToInstance.get("us-central1-a"), Integer.valueOf(2));
    assertFalse(terminationZoneToInstance.containsKey("us-central1-b"));
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
}
