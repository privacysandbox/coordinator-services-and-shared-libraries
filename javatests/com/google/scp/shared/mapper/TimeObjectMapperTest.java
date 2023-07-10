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

package com.google.scp.shared.mapper;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.mapper.TimeObjectMapper.SERIALIZATION_ERROR_RESPONSE_JSON;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.operator.frontend.testing.TestSerializedObject;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.model.Code;
import java.time.Instant;
import org.junit.Before;
import org.junit.Test;

public class TimeObjectMapperTest {

  private TimeObjectMapper target;

  @Before
  public void setUp() {
    target = new TimeObjectMapper();
  }

  @Test
  public void propertiesSerializedToProperFormats() throws JsonProcessingException {
    TestSerializedObject testObject =
        TestSerializedObject.builder()
            .timestamp(Instant.parse("2021-12-03T10:15:30.00Z"))
            .name("james")
            .build();

    String result = target.writeValueAsString(testObject);

    assertThat(result)
        .isEqualTo("{\"timestamp\":\"2021-12-03T10:15:30Z\",\"name\":\"james\",\"address\":null}");
  }

  @Test
  // TODO: Move this to a package for shared constants.
  public void serializationErrorResponseJson_isParsable() throws InvalidProtocolBufferException {
    ErrorResponse.Builder builder = ErrorResponse.newBuilder();
    JsonFormat.parser().merge(SERIALIZATION_ERROR_RESPONSE_JSON, builder);
    ErrorResponse errorResponse = builder.build();

    assertThat(errorResponse.getCode()).isEqualTo(Code.INTERNAL.getRpcStatusCode());
    assertThat(errorResponse.getMessage()).isEqualTo("Internal serialization error");
    assertThat(errorResponse.getDetailsCount()).isEqualTo(1);
    assertThat(errorResponse.getDetails(0).getReason()).isEqualTo("INTERNAL_ERROR");
  }
}
