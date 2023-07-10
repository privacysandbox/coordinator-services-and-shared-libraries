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

import static com.google.common.truth.Truth8.assertThat;

import com.amazonaws.services.lambda.runtime.events.DynamodbEvent;
import com.amazonaws.services.lambda.runtime.events.DynamodbEvent.DynamodbStreamRecord;
import com.amazonaws.services.lambda.runtime.events.models.dynamodb.AttributeValue;
import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.Maps;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import java.time.Instant;
import java.util.Optional;
import org.junit.Rule;
import org.junit.Test;

public class DynamoStreamsJobMetadataUpdateCheckerTest {

  @Rule public final Acai acai = new Acai(TestEnv.class);

  @Inject DynamoStreamsJobMetadataUpdateChecker updateChecker;

  @Test
  public void testCheckForUpdatedMetadata_returnsEmptyForMissingNewImage() {
    DynamodbStreamRecord dynamodbStreamRecord = JobGenerator.createFakeDynamodbStreamRecord("foo");
    dynamodbStreamRecord.getDynamodb().setNewImage(null);

    Optional<JobMetadata> update = updateChecker.checkForUpdatedMetadata(dynamodbStreamRecord);

    assertThat(update).isEmpty();
  }

  @Test
  public void testCheckForUpdatedMetadata_returnsNewImageIfOldMissing() {
    DynamodbStreamRecord dynamodbStreamRecord = JobGenerator.createFakeDynamodbStreamRecord("foo");
    dynamodbStreamRecord.getDynamodb().setOldImage(null);
    JobMetadata expected = JobGenerator.createFakeJobMetadata("foo");

    Optional<JobMetadata> update = updateChecker.checkForUpdatedMetadata(dynamodbStreamRecord);

    assertThat(update).isPresent();
    assertThat(update).hasValue(expected);
  }

  @Test
  public void testCheckForUpdatedMetadata_returnsNewImageIfOldIsDifferent() {
    DynamodbStreamRecord dynamodbStreamRecord = JobGenerator.createFakeDynamodbStreamRecord("foo");
    JobMetadata expected = JobGenerator.createFakeJobMetadata("foo");

    Optional<JobMetadata> update = updateChecker.checkForUpdatedMetadata(dynamodbStreamRecord);

    assertThat(update).isPresent();
    assertThat(update).hasValue(expected);
  }

  @Test
  public void testCheckForUpdatedMetadata_returnsEmptyIfNewIsSameAsOld() {
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

    Optional<JobMetadata> update = updateChecker.checkForUpdatedMetadata(dynamodbStreamRecord);

    assertThat(update).isEmpty();
  }

  public static class TestEnv extends AbstractModule {}
}
