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

package com.google.scp.coordinator.keymanagement.keyhosting.service.common;

/** Defines helper methods for Key Hosting Services */
public final class KeyHostingUtil {

  private KeyHostingUtil() {}

  /**
   * Cache-Control header key as defined in
   * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cache-Control
   */
  public static final String CACHE_CONTROL_HEADER_NAME = "Cache-Control";

  /**
   * Cache-Control max-age directive prefix as defined in
   * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cache-Control
   */
  public static final String MAX_AGE_PREFIX = "max-age=";

  /** Returns String value of Cache-Control max-age directive in format 'max-age=%maxAgeSeconds' */
  public static String getMaxAgeCacheControlValue(Long maxAgeSeconds) {
    return MAX_AGE_PREFIX + maxAgeSeconds;
  }
}
