/*
 * Copyright 2025 Google LLC
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

package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import static com.google.scp.shared.api.model.HttpMethod.GET;

import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.api.model.HttpMethod;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandler;
import com.google.scp.shared.gcp.util.CloudFunctionServiceBase;
import io.opentelemetry.api.OpenTelemetry;
import java.util.regex.Pattern;

/** Base class for PublicKeyService. */
public abstract class PublicKeyServiceHttpFunctionBase extends CloudFunctionServiceBase {

  private static final String PATTERN_STR =
      String.format(
          "/.well-known/%s/v1/public-keys",
          System.getenv().getOrDefault("APPLICATION_NAME", "default"));
  private static final Pattern GET_PUBLIC_KEYS_URL_PATTERN =
      Pattern.compile(PATTERN_STR, Pattern.CASE_INSENSITIVE);
  private final GetPublicKeysRequestHandler getPublicKeysRequestHandler;

  /**
   * Creates a new instance of the {@code PublicKeyServiceHttpFunctionBase} class with the given
   * {@link GetPublicKeysRequestHandler}.
   */
  public PublicKeyServiceHttpFunctionBase(
      GetPublicKeysRequestHandler getPublicKeysRequestHandler, OpenTelemetry OTel) {
    super(OTel);
    this.getPublicKeysRequestHandler = getPublicKeysRequestHandler;
  }

  @Override
  protected ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
      getRequestHandlerMap() {
    return ImmutableMap.of(
        GET, ImmutableMap.of(GET_PUBLIC_KEYS_URL_PATTERN, this.getPublicKeysRequestHandler));
  }
}
