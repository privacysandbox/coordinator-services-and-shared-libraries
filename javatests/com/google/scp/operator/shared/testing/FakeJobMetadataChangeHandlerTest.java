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

package com.google.scp.operator.shared.testing;

import static com.google.common.truth.Truth.assertThat;

import com.google.common.collect.ImmutableList;
import com.google.scp.operator.frontend.testing.FakeJobMetadataChangeHandler;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class FakeJobMetadataChangeHandlerTest {

  FakeJobMetadataChangeHandler target;

  @Test
  public void givenCanHandleTrue_canHandle_ReturnsTrue() {
    target = new FakeJobMetadataChangeHandler(true);

    JobMetadata jobMetadata = JobGenerator.createFakeJobMetadata("123");

    assertThat(target.canHandle(jobMetadata)).isTrue();
  }

  @Test
  public void givenCanHandleFalse_canHandle_ReturnsFalse() {
    target = new FakeJobMetadataChangeHandler(false);

    JobMetadata jobMetadata = JobGenerator.createFakeJobMetadata("123");

    assertThat(target.canHandle(jobMetadata)).isFalse();
  }

  @Test
  public void handled_ReturnsHandled() {
    target = new FakeJobMetadataChangeHandler(true);
    ImmutableList<JobMetadata> jobMetadata =
        ImmutableList.of(
            JobGenerator.createFakeJobMetadata("123"), JobGenerator.createFakeJobMetadata("1234"));

    target.handle(jobMetadata.get(0));
    target.handle(jobMetadata.get(1));

    assertThat(target.getHandled()).isEqualTo(jobMetadata);
  }
}
