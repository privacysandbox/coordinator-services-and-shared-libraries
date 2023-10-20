package com.google.scp.shared.clients.configclient.gcp;

import com.google.cloud.secretmanager.v1.AccessSecretVersionResponse;
import com.google.cloud.secretmanager.v1.SecretManagerServiceClient;
import com.google.inject.Inject;

/**
 * Client to make calls to SecretManagerService. Proxy class helps with mocking final classes in GCP
 * clients.
 */
public class SecretManagerServiceClientProxy {

  private final SecretManagerServiceClient secretManagerServiceClient;

  @Inject
  public SecretManagerServiceClientProxy(SecretManagerServiceClient secretManagerServiceClient) {
    this.secretManagerServiceClient = secretManagerServiceClient;
  }

  /** Gets secret version for given secretName. */
  public AccessSecretVersionResponse accessSecretVersion(String secretName) {
    return this.secretManagerServiceClient.accessSecretVersion(secretName);
  }
}
