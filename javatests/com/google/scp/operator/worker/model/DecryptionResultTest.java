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

package com.google.scp.operator.worker.model;

import static com.google.scp.operator.protos.shared.backend.JobErrorCategoryProto.JobErrorCategory.GENERAL_ERROR;
import static org.junit.Assert.assertThrows;

import com.google.scp.operator.worker.testing.FakeReportGenerator;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class DecryptionResultTest {

  /** Test that the report can be set without error */
  @Test
  public void build_success() {
    DecryptionResult.Builder resultBuilder =
        DecryptionResult.builder().setReport(FakeReportGenerator.generate(1));

    resultBuilder.build();

    // Test passes if no exceptions are thrown
  }

  /** Test that error messages can be added without error */
  @Test
  public void buildWithErrors_success() {
    DecryptionResult.Builder resultBuilder =
        DecryptionResult.builder()
            .addErrorMessage(
                ErrorMessage.builder()
                    .setCategory(GENERAL_ERROR.name())
                    .setDetailedErrorMessage("")
                    .build());

    resultBuilder.build();

    // Test passes if no exceptions are thrown
  }

  /** Test that an error is thrown if both a record and error messages are provided */
  @Test
  public void build_exceptionRecordAndErrorMessageExist() {
    DecryptionResult.Builder resultBuilder =
        DecryptionResult.builder()
            .setReport(FakeReportGenerator.generate(1))
            .addErrorMessage(
                ErrorMessage.builder()
                    .setCategory(GENERAL_ERROR.name())
                    .setDetailedErrorMessage("")
                    .build());

    // An exception should be thrown as the DecryptionResult is not valid, it contains
    // both a record and error messages.
    assertThrows(IllegalStateException.class, resultBuilder::build);
  }

  /** Test that an error is thrown if neither a report or error messages are provided */
  @Test
  public void build_ExceptionEmptyResult() {
    DecryptionResult.Builder resultBuilder = DecryptionResult.builder();

    // An exception should be thrown as the DecryptionResult is not valid, it contains
    // neither a record or error messages.
    assertThrows(IllegalStateException.class, resultBuilder::build);
  }
}
