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

package com.google.scp.shared.proto;

import com.google.common.collect.ImmutableSet;
import com.google.protobuf.ProtocolMessageEnum;
import com.google.protobuf.Timestamp;
import java.time.Duration;
import java.time.Instant;
import java.util.Arrays;

public final class ProtoUtil {

  private static final String UNRECOGNIZED_VALUE = "UNRECOGNIZED";

  /** Gets all the valid values of a proto enum, in String type. */
  public static <T extends Enum<T> & ProtocolMessageEnum> ImmutableSet<String> getValidEnumValues(
      Class<T> protoEnum) {
    return Arrays.stream(protoEnum.getEnumConstants())
        .map(Enum::toString)
        .filter(s -> !s.equals(UNRECOGNIZED_VALUE))
        .collect(ImmutableSet.toImmutableSet());
  }

  /** Convert from Protobuf Timestamp to Java Instant. */
  public static Instant toJavaInstant(Timestamp timestamp) {
    return Instant.ofEpochSecond(timestamp.getSeconds(), timestamp.getNanos());
  }

  /** Convert from Java Instant to Protobuf Timestamp. */
  public static Timestamp toProtoTimestamp(Instant instant) {
    return Timestamp.newBuilder()
        .setSeconds(instant.getEpochSecond())
        .setNanos(instant.getNano())
        .build();
  }

  /** Convert from Protobuf Duration to Java Duration. */
  public static Duration toJavaDuration(com.google.protobuf.Duration duration) {
    return Duration.ofSeconds(duration.getSeconds(), duration.getNanos());
  }

  /** Convert from Java Duration to Protobuf Duration. */
  public static com.google.protobuf.Duration toProtoDuration(Duration javaDuration) {
    return com.google.protobuf.Duration.newBuilder()
        .setSeconds(javaDuration.getSeconds())
        .setNanos(javaDuration.getNano())
        .build();
  }
}
