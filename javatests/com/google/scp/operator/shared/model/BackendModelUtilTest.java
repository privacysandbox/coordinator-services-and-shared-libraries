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

package com.google.scp.operator.shared.model;

import static com.google.common.truth.Truth.assertThat;

import com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import org.junit.Before;
import org.junit.Test;

public final class BackendModelUtilTest {

  JobMetadata.Builder jobMetadataBuilder;

  @Before
  public void setUp() {
    jobMetadataBuilder = JobGenerator.createFakeJobMetadata("foo").toBuilder();
  }

  @Test
  public void testEqualsIgnoreRecordVersion_equalForSameVersion() {
    JobMetadata jobMetadata = jobMetadataBuilder.build();
    JobMetadata otherJobMetadta = jobMetadataBuilder.build();

    boolean equal = BackendModelUtil.equalsIgnoreDbFields(jobMetadata, otherJobMetadta);

    assertThat(equal).isTrue();
  }

  @Test
  public void testEqualsIgnoreRecordVersion_equalForDiffentVersion() {
    JobMetadata jobMetadata = jobMetadataBuilder.setRecordVersion(4).build();
    JobMetadata otherJobMetadta = jobMetadataBuilder.setRecordVersion(5).build();

    boolean equal = BackendModelUtil.equalsIgnoreDbFields(jobMetadata, otherJobMetadta);

    assertThat(equal).isTrue();
  }

  @Test
  public void testEqualsIgnoreRecordVersion_notEqualForDifferent() {
    JobMetadata jobMetadata = jobMetadataBuilder.setJobStatus(JobStatus.IN_PROGRESS).build();
    JobMetadata otherJobMetadta = jobMetadataBuilder.setJobStatus(JobStatus.FINISHED).build();

    boolean equal = BackendModelUtil.equalsIgnoreDbFields(jobMetadata, otherJobMetadta);

    assertThat(equal).isFalse();
  }
}
