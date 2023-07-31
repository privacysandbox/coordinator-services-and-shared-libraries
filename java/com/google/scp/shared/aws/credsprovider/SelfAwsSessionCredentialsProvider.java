/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.shared.aws.credsprovider;

import java.util.Optional;
import javax.inject.Singleton;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.AwsSessionCredentials;
import software.amazon.awssdk.services.sts.StsClient;
import software.amazon.awssdk.services.sts.auth.StsGetSessionTokenCredentialsProvider;

/**
 * AWS Session credentials provider which returns the current credentials if they are already
 * temporary session credentials or else falls back to using the provided {@code StsClient}'s
 * credentials to generate new temporary session credentials.
 *
 * <p>Used when a session token is needed but a role doesn't need to be assumed (e.g. when using
 * {@link com.google.scp.shared.crypto.tink.kmstoolenclave.KmsToolEnclaveAead})
 *
 * <p>To assume a role use {@link StsAwsSessionCredentialsProvider} instead.
 *
 * <p>Because of the fallback behavior, this session credentials provider is compatible both with
 * static credentials providers (e.g. environment variable or manually configured credentials) which
 * do not have a session token and temporary credentials providers (e.g. instance profile
 * credentials) which have a session token and cannot be used to directly fetch new session tokens.
 */
@Singleton
public final class SelfAwsSessionCredentialsProvider extends AwsSessionCredentialsProvider {
  /** Duration for which requested temporary credentials should be valid. */
  private static final int DURATION_SECONDS = 1000;

  private final AwsCredentialsProvider credentialsProvider;

  /**
   * @param processCredentialsProvider The credential provider belonging to the Java process.
   * @param stsClient Used to fetch temporary credentials if {@code processCredentialsProvider} does
   *     not resolve temporary credentials.
   */
  public SelfAwsSessionCredentialsProvider(
      AwsCredentialsProvider processCredentialsProvider, StsClient stsClient) {
    super(
        // This provider will fail if stsClient is already configured to use session credentials
        // because session credentials cannot be used with getSessionToken.
        StsGetSessionTokenCredentialsProvider.builder()
            .stsClient(stsClient)
            .refreshRequest(request -> request.durationSeconds(DURATION_SECONDS))
            .build(),
        () ->
            stsClient
                .getSessionToken(request -> request.durationSeconds(DURATION_SECONDS))
                .credentials());
    this.credentialsProvider = processCredentialsProvider;
  }

  @Override
  public AwsSessionCredentials resolveCredentials() {
    return resolveProcessCredentials().orElseGet(super::resolveCredentials);
  }

  /** Resolves current credentials and returns them if they are session credentials. */
  private Optional<AwsSessionCredentials> resolveProcessCredentials() {
    var credentials = credentialsProvider.resolveCredentials();

    if (credentials instanceof AwsSessionCredentials) {
      return Optional.of((AwsSessionCredentials) credentials);
    }
    return Optional.empty();
  }
}
