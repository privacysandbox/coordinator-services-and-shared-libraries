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

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import com.fasterxml.jackson.datatype.guava.GuavaModule;
import com.fasterxml.jackson.datatype.jdk8.Jdk8Module;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;

/** Object mapper for Guava objects with time module */
public final class TimeObjectMapper extends ObjectMapper {

  /**
   * JSON string to use in API responses when there is a serialization error. Useful to avoid
   * serializing an ErrorResponse of a serialization error
   */
  // TODO: Move this to a package for shared constants.
  public static final String SERIALIZATION_ERROR_RESPONSE_JSON =
      "{\"code\":13,\"message\":\"Internal serialization"
          + " error\",\"details\":[{\"reason\":\"INTERNAL_ERROR\"}]}";

  public TimeObjectMapper() {
    super();
    registerModule(new GuavaModule());
    registerModule(new JavaTimeModule());
    registerModule(new Jdk8Module());
    configure(SerializationFeature.WRITE_DATES_AS_TIMESTAMPS, false);
  }
}
