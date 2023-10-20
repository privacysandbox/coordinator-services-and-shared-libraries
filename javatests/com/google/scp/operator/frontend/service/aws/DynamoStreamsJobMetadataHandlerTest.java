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

package com.google.scp.operator.frontend.service.aws;

import static com.google.common.truth.Truth.assertThat;

import com.amazonaws.services.lambda.runtime.events.DynamodbEvent;
import com.amazonaws.services.lambda.runtime.events.DynamodbEvent.DynamodbStreamRecord;
import com.amazonaws.services.lambda.runtime.events.models.dynamodb.AttributeValue;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.Maps;
import com.google.scp.operator.frontend.testing.FakeJobMetadataChangeHandler;
import com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.operator.shared.testing.TestBaseAwsChangeHandlerModule;
import java.time.Instant;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class DynamoStreamsJobMetadataHandlerTest {

  DynamoStreamsJobMetadataHandler target;

  @Before
  public void setUp() {
    target = new DynamoStreamsJobMetadataHandler();

    // The fake change handlers aren't recreated each test so they must be reset before tests
    TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.forEach(
        FakeJobMetadataChangeHandler::reset);
  }

  @Test
  public void onlySpecifiedChangeHandlersCalled_whenJobMetadataChanged() {
    ImmutableList<DynamodbStreamRecord> streamRecords =
        ImmutableList.of(
            JobGenerator.createFakeDynamodbStreamRecord("1"),
            JobGenerator.createFakeDynamodbStreamRecord("2"));
    ImmutableList<JobMetadata> expectedJobMetadata =
        ImmutableList.of(
            JobGenerator.createFakeJobMetadata("1"), JobGenerator.createFakeJobMetadata("2"));
    DynamodbEvent dynamodbEvent = new DynamodbEvent();
    dynamodbEvent.setRecords(streamRecords);

    target.handleRequest(dynamodbEvent, /*context*/ null);

    // First handler does not handle either record
    assertThat(TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.get(0).getHandled())
        .isEmpty();
    // Second handler handles both records
    assertThat(TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.get(1).getHandled())
        .containsExactlyElementsIn(expectedJobMetadata);
    // Third handler handles both records
    assertThat(TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.get(2).getHandled())
        .containsExactlyElementsIn(expectedJobMetadata);
  }

  @Test
  public void onlySpecifiedChangeHandlersCalled_whenJobMetadataIsNew() {
    DynamodbStreamRecord dynamodbStreamRecord = JobGenerator.createFakeDynamodbStreamRecord("foo");
    // Set the old image to null so that only the new image is present
    dynamodbStreamRecord.getDynamodb().setOldImage(null);
    DynamodbEvent dynamodbEvent = new DynamodbEvent();
    dynamodbEvent.setRecords(ImmutableList.of(dynamodbStreamRecord));
    JobMetadata expectedJobMetadataHandled = JobGenerator.createFakeJobMetadata("foo");

    target.handleRequest(dynamodbEvent, /*context*/ null);

    // First handler does not handle the record
    assertThat(TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.get(0).getHandled())
        .isEmpty();
    // Second handler handles the record
    assertThat(TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.get(1).getHandled())
        .containsExactly(expectedJobMetadataHandled);
    // Third handler handles both records
    assertThat(TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.get(2).getHandled())
        .containsExactly(expectedJobMetadataHandled);
  }

  @Test
  public void changeHandlerNotCalled_whenEventHasNoChanges() {
    // Create a DynamodbEvent that contains one stream record with only a change to the
    // RecordVersion.
    DynamodbStreamRecord dynamodbStreamRecord = JobGenerator.createFakeDynamodbStreamRecord("foo");
    // Update the old image's fields to match the new image's by setting jobStatus=IN_PROGRESS, and
    // requestUpdatedAt to proper value
    ImmutableMap<String, AttributeValue> existingOldImage =
        ImmutableMap.copyOf(dynamodbStreamRecord.getDynamodb().getOldImage());
    ImmutableMap<String, AttributeValue> updates =
        ImmutableMap.of(
            "JobStatus",
            new AttributeValue().withS(JobStatus.IN_PROGRESS.toString()),
            "RequestUpdatedAt",
            new AttributeValue().withS(Instant.parse("2019-10-01T08:29:24.00Z").toString()));
    ImmutableMap<String, AttributeValue> updatedOldImage =
        ImmutableMap.<String, AttributeValue>builder()
            .putAll(Maps.difference(existingOldImage, updates).entriesOnlyOnLeft())
            .putAll(updates)
            .build();
    dynamodbStreamRecord.getDynamodb().setOldImage(updatedOldImage);
    DynamodbEvent dynamodbEvent = new DynamodbEvent();
    dynamodbEvent.setRecords(ImmutableList.of(dynamodbStreamRecord));

    target.handleRequest(dynamodbEvent, /*context*/ null);

    assertThat(TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.get(0).getHandled())
        .isEmpty();
    assertThat(TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.get(1).getHandled())
        .isEmpty();
    assertThat(TestBaseAwsChangeHandlerModule.jobMetadataChangeHandlers.get(2).getHandled())
        .isEmpty();
  }
}
