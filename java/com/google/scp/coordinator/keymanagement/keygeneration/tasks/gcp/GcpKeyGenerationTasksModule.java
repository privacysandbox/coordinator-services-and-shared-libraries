package com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp;

import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.GcpKeyGenerationUtil.getAead;
import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.GcpKeyGenerationUtil.getAeadFromEncodedKeysetHandle;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KMS_KEY_URI;

import com.google.crypto.tink.Aead;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.clients.configclient.gcp.GcpCoordinatorClientConfig;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import java.util.Optional;

/**
 * Module for Business Layer bindings used for KeyGeneration. Handles single party key generation.
 *
 * @deprecated Single-party key features are deprecated. Pending removal b/282204533.
 */
@Deprecated
public final class GcpKeyGenerationTasksModule extends AbstractModule {

  private final String kmsKeyUri;
  private final Optional<String> encodedKeysetHandle;

  public GcpKeyGenerationTasksModule(String kmsKeyUri, Optional<String> encodedKeysetHandle) {
    this.kmsKeyUri = kmsKeyUri;
    this.encodedKeysetHandle = encodedKeysetHandle;
  }

  /** Provides the kmsKeyUri for this coordinator from either args or parameter client. */
  @Provides
  @Singleton
  @KeyEncryptionKeyUri
  String provideKeyEncryptionKeyUri(ParameterClient parameterClient)
      throws ParameterClientException {
    return parameterClient.getParameter(KMS_KEY_URI).orElse(kmsKeyUri);
  }

  /** Provides a {@code KmsClient} for this coordinator. */
  @Provides
  @Singleton
  @KmsKeyAead
  Aead provideAead(@KeyEncryptionKeyUri String kmsKeyUri) {
    return encodedKeysetHandle.isPresent()
        ? getAeadFromEncodedKeysetHandle(encodedKeysetHandle.get())
        : getAead(kmsKeyUri, Optional.empty());
  }

  @Override
  protected void configure() {
    bind(GcpCoordinatorClientConfig.class).toInstance(GcpCoordinatorClientConfig.builder().build());
  }
}
