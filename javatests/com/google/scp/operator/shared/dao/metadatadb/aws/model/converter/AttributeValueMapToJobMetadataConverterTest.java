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

package com.google.scp.operator.shared.dao.metadatadb.aws.model.converter;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.protos.shared.backend.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.shared.backend.JobKeyProto.JobKey;
import com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.shared.proto.ProtoUtil;
import java.time.Instant;
import java.util.Map;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import software.amazon.awssdk.enhanced.dynamodb.internal.AttributeValues;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;

public class AttributeValueMapToJobMetadataConverterTest {

  private static final String JOB_PARAM_ATTRIBUTION_REPORT_TO = "attribution_report_to";
  private static final String JOB_PARAM_OUTPUT_DOMAIN_BLOB_PREFIX = "output_domain_blob_prefix";
  private static final String JOB_PARAM_OUTPUT_DOMAIN_BUCKET_NAME = "output_domain_bucket_name";
  private static final String JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT = "debug_privacy_budget_limit";

  @Rule public final Acai acai = new Acai(TestEnv.class);

  @Inject AttributeValueMapToJobMetadataConverter attributeValueMapToJobMetadataConverter;

  private Map<String, AttributeValue> attributeValueMapNoVersion;
  private Map<String, AttributeValue> attributeValueMapWithVersion;
  private JobMetadata jobMetadataNoVersion;
  private JobMetadata jobMetadataWithVersion;

  @Before()
  public void setUp() {
    String jobRequestId = UUID.randomUUID().toString();
    String version = "version1.0";
    String dataHandle = "dataHandle";
    String dataHandleBucket = "dataHandleBucket";
    String postbackUrl = "postbackUrl";
    ImmutableList<String> privacyBudgetKeys =
        ImmutableList.of("privacyBudgetKey1", "privacyBudgetKey2");
    String attributionDestination = "mysite.com";
    String attributionReportTo = "foo.com";
    Integer debugPrivacyBudgetLimit = 5;
    Instant requestReceivedAt = Instant.parse("2019-10-01T08:25:24.00Z");
    int numAttempts = 0;
    JobStatus jobStatus = JobStatus.FINISHED;
    ImmutableList<Instant> reportingWindows =
        ImmutableList.of(
            Instant.parse("2019-09-01T02:00:00.00Z"), Instant.parse("2019-09-01T02:30:00.00Z"));
    int recordVersion = 4;

    JobKey jobKey = JobKey.newBuilder().setJobRequestId(jobRequestId).build();

    ImmutableMap<String, AttributeValue> createJobRequestMap =
        ImmutableMap.<String, AttributeValue>builder()
            .put("JobRequestId", AttributeValues.stringValue(jobRequestId))
            .put("Version", AttributeValues.stringValue(version))
            .put("InputDataBlobPrefix", AttributeValues.stringValue(dataHandle))
            .put("InputDataBlobBucket", AttributeValues.stringValue(dataHandleBucket))
            .put("OutputDataBlobPrefix", AttributeValues.stringValue(dataHandle))
            .put("OutputDataBlobBucket", AttributeValues.stringValue(dataHandleBucket))
            .put("PostbackUrl", AttributeValues.stringValue(postbackUrl))
            .put(
                "PrivacyBudgetKeys",
                AttributeValue.builder()
                    .l(
                        privacyBudgetKeys.stream()
                            .map(AttributeValues::stringValue)
                            .collect(toImmutableList()))
                    .build())
            .put("AttributionDestination", AttributeValues.stringValue(attributionDestination))
            .put("AttributionReportTo", AttributeValues.stringValue(attributionReportTo))
            .put(
                "ReportingWindows",
                AttributeValue.builder()
                    .l(
                        reportingWindows.stream()
                            .map(Instant::toString)
                            .map(AttributeValues::stringValue)
                            .collect(toImmutableList()))
                    .build())
            .put("DebugPrivacyBudgetLimit", AttributeValues.numberValue(debugPrivacyBudgetLimit))
            .put(
                "JobParameters",
                AttributeValue.builder()
                    .m(
                        ImmutableMap.of(
                            JOB_PARAM_ATTRIBUTION_REPORT_TO,
                            AttributeValues.stringValue(attributionReportTo),
                            JOB_PARAM_OUTPUT_DOMAIN_BLOB_PREFIX,
                            AttributeValues.stringValue(dataHandle),
                            JOB_PARAM_OUTPUT_DOMAIN_BUCKET_NAME,
                            AttributeValues.stringValue(dataHandleBucket),
                            JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT,
                            AttributeValues.stringValue(debugPrivacyBudgetLimit.toString())))
                    .build())
            .build();

    ImmutableMap.Builder<String, AttributeValue> attributeValueMapBuilder =
        ImmutableMap.<String, AttributeValue>builder()
            .put("JobKey", AttributeValues.stringValue(jobKey.getJobRequestId()))
            .put("CreateJobRequest", AttributeValue.builder().m(createJobRequestMap).build())
            .put("RequestReceivedAt", AttributeValues.stringValue(requestReceivedAt.toString()))
            .put("RequestUpdatedAt", AttributeValues.stringValue(requestReceivedAt.toString()))
            .put("NumAttempts", AttributeValues.numberValue(numAttempts))
            .put("JobStatus", AttributeValues.stringValue(jobStatus.toString()));

    attributeValueMapNoVersion = attributeValueMapBuilder.build();

    attributeValueMapWithVersion =
        attributeValueMapBuilder
            .put("RecordVersion", AttributeValues.numberValue(recordVersion))
            .build();

    CreateJobRequest createJobRequest =
        CreateJobRequest.newBuilder()
            .setJobRequestId(jobRequestId)
            .setInputDataBlobPrefix(dataHandle)
            .setInputDataBucketName(dataHandleBucket)
            .setOutputDataBlobPrefix(dataHandle)
            .setOutputDataBucketName(dataHandleBucket)
            .setPostbackUrl(postbackUrl)
            .setAttributionReportTo(attributionReportTo)
            .setDebugPrivacyBudgetLimit(debugPrivacyBudgetLimit)
            .putAllJobParameters(
                ImmutableMap.of(
                    JOB_PARAM_ATTRIBUTION_REPORT_TO,
                    attributionReportTo,
                    JOB_PARAM_OUTPUT_DOMAIN_BLOB_PREFIX,
                    dataHandle,
                    JOB_PARAM_OUTPUT_DOMAIN_BUCKET_NAME,
                    dataHandleBucket,
                    JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT,
                    debugPrivacyBudgetLimit.toString()))
            .build();

    JobMetadata.Builder jobMetadataBuilder =
        JobMetadata.newBuilder()
            .setCreateJobRequest(createJobRequest)
            .setJobStatus(jobStatus)
            .setRequestReceivedAt(ProtoUtil.toProtoTimestamp(requestReceivedAt))
            .setRequestUpdatedAt(ProtoUtil.toProtoTimestamp(requestReceivedAt))
            .setNumAttempts(0)
            .setJobKey(jobKey);

    jobMetadataNoVersion = jobMetadataBuilder.build();

    jobMetadataWithVersion = jobMetadataBuilder.setRecordVersion(recordVersion).build();
  }

  @Test
  public void testForward_noRecordVersion() {
    // No setup

    JobMetadata converted =
        attributeValueMapToJobMetadataConverter.convert(attributeValueMapNoVersion);

    assertThat(converted).isEqualTo(jobMetadataNoVersion);
  }

  @Test
  public void testForward_withRecordVersion() {
    // No setup

    JobMetadata converted =
        attributeValueMapToJobMetadataConverter.convert(attributeValueMapWithVersion);

    assertThat(converted).isEqualTo(jobMetadataWithVersion);
  }

  @Test
  public void testBackwardThrows() {
    assertThrows(
        UnsupportedOperationException.class,
        () -> attributeValueMapToJobMetadataConverter.reverse().convert(jobMetadataNoVersion));
  }

  public static class TestEnv extends AbstractModule {}
}
