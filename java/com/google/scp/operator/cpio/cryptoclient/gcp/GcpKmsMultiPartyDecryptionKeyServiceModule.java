package com.google.scp.operator.cpio.cryptoclient.gcp;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.crypto.tink.integration.gcpkms.GcpKmsClient;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorAHttpClient;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBHttpClient;
import com.google.scp.operator.cpio.configclient.gcp.Annotations.CoordinatorACredentials;
import com.google.scp.operator.cpio.configclient.gcp.Annotations.CoordinatorBCredentials;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorAAead;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorAEncryptionKeyServiceBaseUrl;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorBAead;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorBEncryptionKeyServiceBaseUrl;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceModule;
import com.google.scp.operator.cpio.cryptoclient.EncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.HttpEncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.MultiPartyDecryptionKeyServiceImpl;
import com.google.scp.operator.cpio.cryptoclient.MultiPartyDecryptionKeyServiceImpl.CoordinatorAEncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.MultiPartyDecryptionKeyServiceImpl.CoordinatorBEncryptionKeyFetchingService;
import com.google.scp.shared.api.util.HttpClientWrapper;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import java.security.GeneralSecurityException;

/**
 * Guice module for implementation of {@link DecryptionKeyService} which fetches private key splits
 * from multiple parties and decrypts the private key splits using gcloud KMS.
 */
public final class GcpKmsMultiPartyDecryptionKeyServiceModule extends DecryptionKeyServiceModule {

  /** Caller is expected to bind {@link GcpKmsDecryptionKeyServiceConfig}. */
  public GcpKmsMultiPartyDecryptionKeyServiceModule() {}

  @Override
  public Class<? extends DecryptionKeyService> getDecryptionKeyServiceImplementation() {
    return MultiPartyDecryptionKeyServiceImpl.class;
  }

  @Provides
  @CoordinatorAEncryptionKeyFetchingService
  @Singleton
  public EncryptionKeyFetchingService provideCoordinatorAEncryptionKeyFetchingService(
      @CoordinatorAHttpClient HttpClientWrapper httpClient,
      @CoordinatorAEncryptionKeyServiceBaseUrl String coordinatorAEncryptionKeyServiceBaseUrl) {
    return new HttpEncryptionKeyFetchingService(
        httpClient, coordinatorAEncryptionKeyServiceBaseUrl);
  }

  @Provides
  @CoordinatorBEncryptionKeyFetchingService
  @Singleton
  public EncryptionKeyFetchingService provideCoordinatorBEncryptionKeyFetchingService(
      @CoordinatorBHttpClient HttpClientWrapper httpClient,
      @CoordinatorBEncryptionKeyServiceBaseUrl String coordinatorBEncryptionKeyServiceBaseUrl) {
    return new HttpEncryptionKeyFetchingService(
        httpClient, coordinatorBEncryptionKeyServiceBaseUrl);
  }

  /** Provides a {@code KmsClient} for coordinator A. */
  @Provides
  @Singleton
  @CoordinatorAAead
  CloudAeadSelector provideCoordinatorAKmsClient(
      @CoordinatorACredentials GoogleCredentials credentials,
      GcpKmsDecryptionKeyServiceConfig gcpKmsDecryptionKeyServiceConfig) {
    if (gcpKmsDecryptionKeyServiceConfig.coordinatorAAead().isPresent()) {
      return (unused) -> gcpKmsDecryptionKeyServiceConfig.coordinatorAAead().get();
    }
    return (kmsKeyArn) -> {
      GcpKmsClient client = new GcpKmsClient();
      try {
        client.withCredentials(credentials);
        return client.getAead(kmsKeyArn);
      } catch (GeneralSecurityException e) {
        throw new RuntimeException(
            String.format("Error getting gcloud Aead with uri %s.", kmsKeyArn), e);
      }
    };
  }

  /** Provides a {@code KmsClient} for coordinator B. */
  @Provides
  @Singleton
  @CoordinatorBAead
  CloudAeadSelector provideCoordinatorBKmsClient(
      @CoordinatorBCredentials GoogleCredentials credentials,
      GcpKmsDecryptionKeyServiceConfig gcpKmsDecryptionKeyServiceConfig) {
    if (gcpKmsDecryptionKeyServiceConfig.coordinatorBAead().isPresent()) {
      return (unused) -> gcpKmsDecryptionKeyServiceConfig.coordinatorBAead().get();
    }
    return (kmsKeyArn) -> {
      GcpKmsClient client = new GcpKmsClient();
      try {
        client.withCredentials(credentials);
        return client.getAead(kmsKeyArn);
      } catch (GeneralSecurityException e) {
        throw new RuntimeException(
            String.format("Error getting gcloud Aead with uri %s.", kmsKeyArn), e);
      }
    };
  }
}
