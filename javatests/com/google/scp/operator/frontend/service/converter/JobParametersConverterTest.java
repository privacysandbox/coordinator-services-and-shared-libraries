/*
 * Copyright 2024 Google LLC
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

package com.google.scp.operator.frontend.service.converter;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableMap;
import com.google.scp.operator.protos.shared.backend.JobParametersProto;
import com.google.scp.operator.shared.utils.ObjectConversionException;
import java.util.Map;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class JobParametersConverterTest {

  private static final JobParametersConverter converter = new JobParametersConverter();

  private static final JobParametersProto.JobParameters JOB_PARAMETERS_OBJECT =
      JobParametersProto.JobParameters.newBuilder()
          .setAttributionReportTo("abc.com")
          .setOutputDomainBlobPrefix("bar")
          .setOutputDomainBucketName("foo")
          .setReportingSite("test value")
          .setDebugPrivacyEpsilon(0D)
          .setReportErrorThresholdPercentage(1.23)
          .setInputReportCount(123)
          .setFilteringIds("test value")
          .setDebugRun(true)
          .build();
  private static final ImmutableMap<String, String> JOB_PARAMETERS_MAP =
      ImmutableMap.of(
          "output_domain_bucket_name", "foo",
          "attribution_report_to", "abc.com",
          "output_domain_blob_prefix", "bar",
          "reporting_site", "test value",
          "debug_privacy_epsilon", "0.0",
          "report_error_threshold_percentage", "1.23",
          "input_report_count", "123",
          "filtering_ids", "test value",
          "debug_run", "true");

  @Test
  public void doForward_success() {
    JobParametersProto.JobParameters expected = JOB_PARAMETERS_OBJECT.toBuilder().build();

    JobParametersProto.JobParameters result = converter.convert(JOB_PARAMETERS_MAP);

    assertThat(result).isEqualTo(expected);
  }

  @Test
  public void doForward_wrongValue() {
    JobParametersProto.JobParameters expected =
        JOB_PARAMETERS_OBJECT.toBuilder().setDebugRun(false).build();

    JobParametersProto.JobParameters result = converter.convert(JOB_PARAMETERS_MAP);

    assertThat(result).isNotEqualTo(expected);
  }

  @Test
  public void doForward_NullParameter() {
    JobParametersProto.JobParameters expected = JOB_PARAMETERS_OBJECT.toBuilder().clear().build();

    JobParametersProto.JobParameters result =
        converter.convert(ImmutableMap.of("output_domain_bucket_name", ""));

    assertThat(result).isNotEqualTo(expected);
    assertThat(expected.hasOutputDomainBucketName()).isEqualTo(false);
    assertThat(result.hasOutputDomainBucketName()).isEqualTo(true);
  }

  @Test
  public void doForward_invalidValueFailure() {
    ImmutableMap<String, String> invalidJobParametersMap =
        ImmutableMap.of("debug_privacy_epsilon", "invalid_value");

    var exception =
        assertThrows(
            ObjectConversionException.class, () -> converter.convert(invalidJobParametersMap));

    assertThat(exception)
        .hasMessageThat()
        .isEqualTo("Invalid value for field debug_privacy_epsilon: invalid_value");
  }

  @Test
  public void doForward_invalidParameterFailure() {
    ImmutableMap<String, String> invalidJobParametersMap =
        ImmutableMap.<String, String>builder()
            .putAll(JOB_PARAMETERS_MAP)
            .put("invalid_parameter", "invalid_value")
            .build();

    var exception =
        assertThrows(
            ObjectConversionException.class, () -> converter.convert(invalidJobParametersMap));

    assertThat(exception)
        .hasMessageThat()
        .isEqualTo("Invalid key: invalid_parameter in job parameters.");
  }

  @Test
  public void doBackward_success() {

    Map<String, String> result = converter.reverse().convert(JOB_PARAMETERS_OBJECT);

    assertThat(result).isEqualTo(JOB_PARAMETERS_MAP);
  }

  @Test
  public void doBackward_successEmptyMap() {

    Map<String, String> result =
        converter.reverse().convert(JobParametersProto.JobParameters.getDefaultInstance());

    assertThat(result).isEmpty();
  }

  @Test
  public void doBackward_successEmptyString() {
    ImmutableMap<String, String> expected = ImmutableMap.of("output_domain_bucket_name", "");

    Map<String, String> result =
        converter
            .reverse()
            .convert(
                JOB_PARAMETERS_OBJECT.toBuilder().clear().setOutputDomainBucketName("").build());

    assertThat(result).isEqualTo(expected);
    assertThat(result).hasSize(1);
  }
}
