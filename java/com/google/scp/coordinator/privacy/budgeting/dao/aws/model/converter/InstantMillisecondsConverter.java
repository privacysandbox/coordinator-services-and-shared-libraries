/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.privacy.budgeting.dao.aws.model.converter;

import java.time.Instant;
import software.amazon.awssdk.enhanced.dynamodb.AttributeConverter;
import software.amazon.awssdk.enhanced.dynamodb.AttributeValueType;
import software.amazon.awssdk.enhanced.dynamodb.EnhancedType;
import software.amazon.awssdk.enhanced.dynamodb.internal.AttributeValues;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;

/**
 * Converts from {@link java.time.Instant} to a DynamoDB Number type where the number represents the
 * timestamp in milliseconds.
 */
public final class InstantMillisecondsConverter implements AttributeConverter<Instant> {

  @Override
  public AttributeValue transformFrom(Instant instant) {
    return instant == null
        ? AttributeValues.nullAttributeValue()
        : AttributeValues.numberValue(instant.toEpochMilli());
  }

  @Override
  public Instant transformTo(AttributeValue attributeValue) {
    return attributeValue.n() == null
        ? Instant.ofEpochMilli(0)
        : Instant.ofEpochMilli(Long.parseLong(attributeValue.n()));
  }

  @Override
  public EnhancedType<Instant> type() {
    return EnhancedType.of(Instant.class);
  }

  @Override
  public AttributeValueType attributeValueType() {
    return AttributeValueType.N;
  }
}
