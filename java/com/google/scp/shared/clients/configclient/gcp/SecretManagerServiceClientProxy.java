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
