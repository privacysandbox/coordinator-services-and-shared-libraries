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

import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.scp.shared.otel.OTelConfigurationModule;
import io.opentelemetry.api.OpenTelemetry;

/** Handles requests to EncryptionKeyService and returns HTTP Response. */
public final class EncryptionKeyServiceHttpFunction extends EncryptionKeyServiceHttpFunctionBase {

  /**
   * Creates a new instance of the {@code EncryptionKeyServiceHttpFunction} class with the given
   * {@link GetEncryptionKeyRequestHandler} and {@link ListRecentEncryptionKeysRequestHandler}.
   */
  public EncryptionKeyServiceHttpFunction(
      GetEncryptionKeyRequestHandler getEncryptionKeyRequestHandler,
      ListRecentEncryptionKeysRequestHandler ListRecentEncryptionKeysRequestHandler,
      OpenTelemetry OTel) {
    super(getEncryptionKeyRequestHandler, ListRecentEncryptionKeysRequestHandler, OTel);
  }

  /** Creates a new instance of the {@code PrivateKeyServiceHttpFunction} class. */
  public EncryptionKeyServiceHttpFunction() {
    this(
        Guice.createInjector(
            new OTelConfigurationModule("EncryptionKeyService"), new GcpKeyServiceModule()));
  }

  /**
   * Creates a new instance of the {@code PrivateKeyServiceHttpFunction} class with the given {@link
   * Injector}.
   */
  private EncryptionKeyServiceHttpFunction(Injector injector) {
    this(
        injector.getInstance(GetEncryptionKeyRequestHandler.class),
        injector.getInstance(ListRecentEncryptionKeysRequestHandler.class),
        injector.getInstance(OpenTelemetry.class));
  }
}
