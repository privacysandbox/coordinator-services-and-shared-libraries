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

package com.google.scp.operator.shared.dao.metadatadb.aws.model.attributeconverter;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.operator.protos.shared.backend.JobErrorCategoryProto.JobErrorCategory.DECRYPTION_ERROR;
import static com.google.scp.operator.protos.shared.backend.JobErrorCategoryProto.JobErrorCategory.GENERAL_ERROR;
import static com.google.scp.operator.protos.shared.backend.JobErrorCategoryProto.JobErrorCategory.HYBRID_KEY_ID_MISSING;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.scp.operator.protos.shared.backend.ErrorCountProto.ErrorCount;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.enhanced.dynamodb.AttributeValueType;
import software.amazon.awssdk.enhanced.dynamodb.EnhancedType;
import software.amazon.awssdk.enhanced.dynamodb.internal.AttributeValues;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;

@RunWith(JUnit4.class)
public class ErrorCountsAttributeConverterTest {

  private ErrorCountsAttributeConverter attributeConverter;

  ImmutableList<ErrorCount> errorCountsInApplication;
  AttributeValue errorCountsInDynamo;

  @Before
  public void setUp() {
    attributeConverter = new ErrorCountsAttributeConverter();

    // Create an errorCounts map the way its represented in our application
    errorCountsInApplication =
        new ImmutableList.Builder<ErrorCount>()
            .add(
                ErrorCount.newBuilder()
                    .setCategory(DECRYPTION_ERROR.name())
                    .setCount(5L)
                    .setDescription("Decryption Error.")
                    .build())
            .add(
                ErrorCount.newBuilder()
                    .setCategory(HYBRID_KEY_ID_MISSING.name())
                    .setCount(8L)
                    .setDescription("Hybrid key is missing.")
                    .build())
            .add(
                ErrorCount.newBuilder()
                    .setCategory(GENERAL_ERROR.name())
                    .setCount(13L)
                    .setDescription("General Error.")
                    .build())
            .build();

    // Create an errorCounts map the way that the data will come back from Dynamo
    ImmutableList<AttributeValue> dynamoList =
        errorCountsInApplication.stream()
            .map(
                errorCount ->
                    AttributeValue.builder()
                        .m(
                            ImmutableMap.of(
                                "Category",
                                AttributeValues.stringValue(errorCount.getCategory().toString()),
                                "Count",
                                AttributeValues.numberValue(errorCount.getCount()),
                                "Description",
                                AttributeValues.stringValue(
                                    errorCount.getDescription().toString())))
                        .build())
            .collect(ImmutableList.toImmutableList());
    errorCountsInDynamo = AttributeValue.builder().l(dynamoList).build();
  }

  @Test
  public void testTransformToDynamoValue() {
    // No setup

    AttributeValue converted = attributeConverter.transformFrom(errorCountsInApplication);

    assertThat(converted).isEqualTo(errorCountsInDynamo);
  }

  @Test
  public void testTransfromFromDynamoValue() {
    // No setup

    List<ErrorCount> converted = attributeConverter.transformTo(errorCountsInDynamo);

    assertThat(converted).containsExactlyElementsIn(errorCountsInApplication);
  }

  @Test
  public void testEnhancedType() {
    // No setup

    EnhancedType<List<ErrorCount>> type = attributeConverter.type();

    assertThat(type).isEqualTo(EnhancedType.listOf(ErrorCount.class));
  }

  @Test
  public void testAttributeValueType() {
    // No setup

    AttributeValueType type = attributeConverter.attributeValueType();

    assertThat(type).isEqualTo(AttributeValueType.L);
  }
}
