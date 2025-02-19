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

package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import static com.google.scp.shared.api.model.HttpMethod.POST;

import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.api.model.HttpMethod;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandler;
import com.google.scp.shared.gcp.util.CloudFunctionServiceBase;
import java.util.regex.Pattern;

/** Base class for KeyStorageService. */
public abstract class KeyStorageServiceHttpFunctionBase extends CloudFunctionServiceBase {

  private static final Pattern CREATE_KEY_URL_PATTERN =
      Pattern.compile("/v1alpha/encryptionKeys", Pattern.CASE_INSENSITIVE);
  private final CreateKeyRequestHandler createKeyRequestHandler;

  /**
   * Creates a new instance of the {@code KeyStorageServiceHttpFunctionBase} class with the given
   * {@link CreateKeyRequestHandler}.
   */
  public KeyStorageServiceHttpFunctionBase(CreateKeyRequestHandler createKeyRequestHandler) {
    this.createKeyRequestHandler = createKeyRequestHandler;
  }

  @Override
  protected ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
      getRequestHandlerMap() {
    return ImmutableMap.of(
        POST, ImmutableMap.of(CREATE_KEY_URL_PATTERN, this.createKeyRequestHandler));
  }
}
