package com.google.scp.operator.shared.dao.metadatadb.aws.model.attributeconverter;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableMap;
import com.google.scp.operator.protos.shared.backend.JobParametersProto;
import com.google.scp.operator.protos.shared.backend.JobParametersProto.JobParameters;
import com.google.scp.operator.shared.utils.ObjectConversionException;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.enhanced.dynamodb.AttributeValueType;
import software.amazon.awssdk.enhanced.dynamodb.EnhancedType;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;

@RunWith(JUnit4.class)
public class JobParametersAttributeConverterTest {

  private JobParametersAttributeConverter attributeConverter;

  private JobParameters jobParametersInApplication;
  private AttributeValue jobParametersInDynamo;
  private ImmutableMap<String, AttributeValue> job_parameters_map;

  @Before
  public void setUp() throws Exception {
    attributeConverter = new JobParametersAttributeConverter();

    // Create a JobParameters object the way is represented in our application.
    jobParametersInApplication =
        JobParameters.newBuilder()
            .setOutputDomainBlobPrefix("test value")
            .setOutputDomainBucketName("test value")
            .setAttributionReportTo("test value")
            .setReportingSite("test value")
            .setDebugPrivacyEpsilon(0D)
            .setReportErrorThresholdPercentage(1.23)
            .setInputReportCount(123)
            .setFilteringIds("test value")
            .setDebugRun(true)
            .build();

    job_parameters_map =
        ImmutableMap.of(
            "output_domain_blob_prefix",
            AttributeValue.fromS("test value"),
            "output_domain_bucket_name",
            AttributeValue.fromS("test value"),
            "attribution_report_to",
            AttributeValue.fromS("test value"),
            "reporting_site",
            AttributeValue.fromS("test value"),
            "debug_privacy_epsilon",
            AttributeValue.fromS("0.0"),
            "report_error_threshold_percentage",
            AttributeValue.fromS("1.23"),
            "input_report_count",
            AttributeValue.fromS("123"),
            "filtering_ids",
            AttributeValue.fromS("test value"),
            "debug_run",
            AttributeValue.fromS("true"));

    // Create a JobParameters map the way that the data will come back from Dynamo.
    jobParametersInDynamo = AttributeValue.builder().m(job_parameters_map).build();
  }

  @Test
  public void transformFromJobParameters() {
    AttributeValue converted = attributeConverter.transformFrom(jobParametersInApplication);

    assertThat(converted).isEqualTo(jobParametersInDynamo);
  }

  @Test
  public void transformFromJobParameters_emptyApplicationObject() {
    AttributeValue expected = AttributeValue.builder().m(ImmutableMap.of()).build();

    AttributeValue result =
        attributeConverter.transformFrom(JobParametersProto.JobParameters.getDefaultInstance());

    assertThat(result).isEqualTo(expected);
    assertThat(result.m()).hasSize(0);
  }

  @Test
  public void transformFromJobParameters_emptyString() {
    AttributeValue expected =
        AttributeValue.builder()
            .m(ImmutableMap.of("output_domain_bucket_name", AttributeValue.fromS("")))
            .build();

    AttributeValue result =
        attributeConverter.transformFrom(
            jobParametersInApplication.toBuilder().clear().setOutputDomainBucketName("").build());

    assertThat(result).isEqualTo(expected);
    assertThat(result.m()).hasSize(1);
  }

  @Test
  public void transformToJobParameters() {
    JobParameters converted = attributeConverter.transformTo(jobParametersInDynamo);

    assertThat(converted).isEqualTo(jobParametersInApplication);
  }

  @Test
  public void transformToJobParameters_wrongValue() {
    JobParametersProto.JobParameters expected =
        jobParametersInApplication.toBuilder().setDebugRun(false).build();

    JobParametersProto.JobParameters result = attributeConverter.transformTo(jobParametersInDynamo);

    assertThat(result).isNotEqualTo(expected);
  }

  @Test
  public void transformToJobParameters_emptyDynamoObject() {
    JobParametersProto.JobParameters expected =
        JobParametersProto.JobParameters.getDefaultInstance();

    AttributeValue emptyDynamoObject = AttributeValue.builder().m(ImmutableMap.of()).build();

    JobParametersProto.JobParameters result = attributeConverter.transformTo(emptyDynamoObject);

    assertThat(result).isEqualTo(expected);
    assertThat(expected.getAllFields()).hasSize(0);
  }

  @Test
  public void transformToJobParameters_invalidValueFailure() {
    AttributeValue invalidDynamoObject =
        AttributeValue.builder()
            .m(ImmutableMap.of("debug_privacy_epsilon", AttributeValue.fromS("invalid_value")))
            .build();

    var exception =
        assertThrows(
            ObjectConversionException.class,
            () -> attributeConverter.transformTo(invalidDynamoObject));

    assertThat(exception)
        .hasMessageThat()
        .isEqualTo("Invalid value for field debug_privacy_epsilon: invalid_value");
  }

  // Backwards-compatibility: Should ignore the invalid field to maintain the job metadata object
  // useful.
  @Test
  public void transformToJobParameters_invalidParameter() {
    JobParametersProto.JobParameters expected = jobParametersInApplication.toBuilder().build();

    AttributeValue invalidDynamoObject =
        AttributeValue.builder()
            .m(
                ImmutableMap.<String, AttributeValue>builder()
                    .putAll(job_parameters_map)
                    .put("invalid_parameter", AttributeValue.fromS("invalid_value"))
                    .build())
            .build();

    JobParametersProto.JobParameters result = attributeConverter.transformTo(invalidDynamoObject);

    assertThat(result).isEqualTo(expected);
  }

  @Test
  public void testEnhancedType() {
    EnhancedType<JobParameters> type = attributeConverter.type();

    assertThat(type).isEqualTo(EnhancedType.of(JobParameters.class));
  }

  @Test
  public void testAttributeValueType() {
    AttributeValueType type = attributeConverter.attributeValueType();

    assertThat(type).isEqualTo(AttributeValueType.M);
  }
}
