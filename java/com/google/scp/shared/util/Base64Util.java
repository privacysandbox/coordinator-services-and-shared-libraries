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

package com.google.scp.shared.util;

import com.google.protobuf.ByteString;
import java.util.Base64;

/** Utility functions for Base64 encodings */
public final class Base64Util {
  private Base64Util() {}

  /**
   * Converts a ByteString to a Base64 encoded String. This is the canonical proto3 JSON
   * serialization of a ByteString.
   */
  public static String toBase64String(ByteString byteString) {
    return new String(Base64.getEncoder().encode(byteString.toByteArray()));
  }

  /**
   * Converts a Base64 encoded String to a ByteString. This is the canonical proto3 JSON
   * serialization of a ByteString.
   */
  public static ByteString fromBase64String(String string) {
    return ByteString.copyFrom(Base64.getDecoder().decode(string));
  }
}
