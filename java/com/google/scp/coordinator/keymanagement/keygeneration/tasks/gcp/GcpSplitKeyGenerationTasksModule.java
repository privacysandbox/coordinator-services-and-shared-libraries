package com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp;

import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.GcpKeyGenerationUtil.getAead;
import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.GcpKeyGenerationUtil.getAeadFromEncodedKeysetHandle;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KMS_KEY_URI;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.crypto.tink.Aead;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.clients.configclient.gcp.Annotations.PeerCoordinatorCredentials;
import com.google.scp.coordinator.clients.configclient.gcp.GcpCoordinatorClientConfig;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.PeerCoordinatorKmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.PeerCoordinatorServiceAccount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.PeerCoordinatorWipProvider;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTask;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.PeerCoordinatorKeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.PeerCoordinatorKmsKeyAead;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import java.util.Optional;

/** Module for Business Layer bindings used for KeyGeneration. Handles multiparty key generation. */
public final class GcpSplitKeyGenerationTasksModule extends AbstractModule {
  private final Optional<String> encodedKeysetHandle;
  private final Optional<String> peerCoordinatorEncodedKeysetHandle;

  public GcpSplitKeyGenerationTasksModule(
      Optional<String> encodedKeysetHandle, Optional<String> peerCoordinatorEncodedKeysetHandle) {
    this.encodedKeysetHandle = encodedKeysetHandle;
    this.peerCoordinatorEncodedKeysetHandle = peerCoordinatorEncodedKeysetHandle;
  }

  /** Provides the kmsKeyUri for this coordinator from either args or parameter client. */
  @Provides
  @Singleton
  @KeyEncryptionKeyUri
  String provideKeyEncryptionKeyUri(ParameterClient parameterClient, @KmsKeyUri String kmsKeyUri)
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

  /** Provides a {@code KmsClient} for peer coordinator. */
  @Provides
  @Singleton
  @PeerCoordinatorKmsKeyAead
  Aead providePeerCoordinatorAead(
      @PeerCoordinatorKeyEncryptionKeyUri String peerCoordinatorKmsKeyUri,
      @PeerCoordinatorCredentials GoogleCredentials credentials) {
    return peerCoordinatorEncodedKeysetHandle.isPresent()
        ? getAeadFromEncodedKeysetHandle(peerCoordinatorEncodedKeysetHandle.get())
        : getAead(peerCoordinatorKmsKeyUri, Optional.of(credentials));
  }

  @Provides
  @Singleton
  @PeerCoordinatorKeyEncryptionKeyUri
  String providesPeerCoordinatorKeyEncryptionKeyUri(
      @PeerCoordinatorKmsKeyUri String peerCoordinatorKmsKeyUri) {
    return peerCoordinatorKmsKeyUri;
  }

  @Provides
  @Singleton
  GcpCoordinatorClientConfig providesGcpCoordinatorClientConfig(
      @PeerCoordinatorWipProvider String peerCoordinatorWipProvider,
      @PeerCoordinatorServiceAccount String peerCoordinatorServiceAccount) {
    GcpCoordinatorClientConfig.Builder configBuilder =
        GcpCoordinatorClientConfig.builder()
            .setPeerCoordinatorServiceAccount(peerCoordinatorServiceAccount)
            .setPeerCoordinatorWipProvider(peerCoordinatorWipProvider)
            .setUseLocalCredentials(peerCoordinatorWipProvider.isEmpty());
    return configBuilder.build();
  }

  @Override
  protected void configure() {
    bind(CreateSplitKeyTask.class).to(GcpCreateSplitKeyTask.class);
  }
}
