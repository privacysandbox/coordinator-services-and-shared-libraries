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

/** Base class for EncryptionKeyService. */
public abstract class EncryptionKeyServiceHttpFunctionBase extends CloudFunctionServiceBase {

  // Example: /v1/encryptionKeys/abc-123
  private static final Pattern GET_ENCRYPTION_KEY_URL_PATTERN =
      Pattern.compile("/v1alpha/encryptionKeys/[^/]*", Pattern.CASE_INSENSITIVE);
  private final GetEncryptionKeyRequestHandler getEncryptionKeyRequestHandler;

  private static final Pattern LIST_RECENT_KEYS_URL_PATTERN =
      Pattern.compile("/v1alpha/encryptionKeys:recent", Pattern.CASE_INSENSITIVE);
  private final ListRecentEncryptionKeysRequestHandler listRecentKeysRequestHandler;

  /**
   * Creates a new instance of the {@code EncryptionKeyServiceHttpFunctionBase} class with the given
   * {@link GetEncryptionKeyRequestHandler} and {@link ListRecentEncryptionKeysRequestHandler}.
   */
  public EncryptionKeyServiceHttpFunctionBase(
      GetEncryptionKeyRequestHandler getEncryptionKeyRequestHandler,
      ListRecentEncryptionKeysRequestHandler listRecentKeysRequestHandler,
      OpenTelemetry OTel) {
    super(OTel);
    this.getEncryptionKeyRequestHandler = getEncryptionKeyRequestHandler;
    this.listRecentKeysRequestHandler = listRecentKeysRequestHandler;
    this.OTel = OTel;
  }

  @Override
  protected ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
      getRequestHandlerMap() {
    return ImmutableMap.of(
        GET,
        ImmutableMap.of(
            GET_ENCRYPTION_KEY_URL_PATTERN,
            this.getEncryptionKeyRequestHandler,
            LIST_RECENT_KEYS_URL_PATTERN,
            this.listRecentKeysRequestHandler));
  }
}
