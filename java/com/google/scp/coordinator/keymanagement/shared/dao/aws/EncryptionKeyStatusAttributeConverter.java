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

package com.google.scp.coordinator.keymanagement.shared.dao.aws;

import com.google.common.collect.ImmutableSet;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb.InvalidEncryptionKeyStatusException;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyStatusProto.EncryptionKeyStatus;
import com.google.scp.shared.proto.ProtoUtil;
import software.amazon.awssdk.enhanced.dynamodb.AttributeConverter;
import software.amazon.awssdk.enhanced.dynamodb.AttributeValueType;
import software.amazon.awssdk.enhanced.dynamodb.EnhancedType;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;

/** Defines DynamoDB converter for {@link EncryptionKeyStatus} and AttributeValue */
public final class EncryptionKeyStatusAttributeConverter
    implements AttributeConverter<EncryptionKeyStatus> {

  public EncryptionKeyStatusAttributeConverter() {}

  public static EncryptionKeyStatusAttributeConverter create() {
    return new EncryptionKeyStatusAttributeConverter();
  }

  /** Transform {@link EncryptionKeyStatus} enum name to Dynamo string type */
  @Override
  public AttributeValue transformFrom(EncryptionKeyStatus input) {
    return AttributeValue.builder().s(input.name()).build();
  }

  /**
   * Transform DynamoDB {@link AttributeValue} string value to {@link EncryptionKeyStatus}. This is
   * only invoked if attributeValue is not null.
   */
  @Override
  public EncryptionKeyStatus transformTo(AttributeValue attributeValue) {
    String attributeValueString = attributeValue.s();
    ImmutableSet<String> values = ProtoUtil.getValidEnumValues(EncryptionKeyStatus.class);
    if (values.contains(attributeValueString))
      return EncryptionKeyStatus.valueOf(attributeValueString);
    else
      throw new InvalidEncryptionKeyStatusException(
          "Unknown EncryptionKeyStatus string found: " + attributeValueString);
  }

  @Override
  public EnhancedType<EncryptionKeyStatus> type() {
    return EnhancedType.of(EncryptionKeyStatus.class);
  }

  @Override
  public AttributeValueType attributeValueType() {
    return AttributeValueType.S;
  }
}
