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
