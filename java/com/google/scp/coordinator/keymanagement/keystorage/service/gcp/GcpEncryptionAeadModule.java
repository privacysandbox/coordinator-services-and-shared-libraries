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

import com.google.crypto.tink.Aead;
import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.shared.util.GcpAeadProvider;
import java.util.Optional;

class GcpEncryptionAeadModule extends AbstractModule {

  private String gcpKmsUri;

  GcpEncryptionAeadModule(String gcpKmsUri) {
    this.gcpKmsUri = gcpKmsUri;
  }

  @Override
  protected void configure() {
    Aead gcpAead = GcpAeadProvider.getGcpAead(gcpKmsUri, Optional.empty());
    bind(Aead.class).annotatedWith(EncryptionAead.class).toInstance(gcpAead);
    bind(String.class).annotatedWith(EncryptionKeyUri.class).toInstance(gcpKmsUri);
  }
}
