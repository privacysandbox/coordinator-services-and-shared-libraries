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

import java.lang.ref.Cleaner;
import java.lang.ref.Cleaner.Cleanable;
import java.util.Optional;
import java.util.function.Supplier;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.AwsSessionCredentials;
import software.amazon.awssdk.services.sts.model.Credentials;
import software.amazon.awssdk.utils.SdkAutoCloseable;

/**
 * An implementation of {@link AwsCredentialsProvider} that provides {@link AwsSessionCredentials}
 * credentials by wrapping {@link AwsCredentialsProvider} that is expected to only provide {@link
 * AwsSessionCredentials} and implicitly asserts on that expectation.
 */
public abstract class AwsSessionCredentialsProvider
    implements AwsCredentialsProvider, SdkAutoCloseable {

  private static final Cleaner cleaner = Cleaner.create();

  private final AwsCredentialsProvider credentialsProvider;
  private final Supplier<AwsSessionCredentials> fallback;
  private final Optional<Cleanable> cleanable;

  /**
   * @param credentialsProvider Known {@link AwsSessionCredentials} providing provider.
   * @param fallback Fallback {@link Credentials} supplier when the <code>credentialsProvider</code>
   *     does not provide the expected {@link AwsSessionCredentials}.
   */
  protected AwsSessionCredentialsProvider(
      AwsCredentialsProvider credentialsProvider, Supplier<Credentials> fallback) {
    this.credentialsProvider = credentialsProvider;
    this.fallback = () -> asSessionCredentials(fallback.get());
    this.cleanable =
        Optional.of(credentialsProvider)
            .filter(SdkAutoCloseable.class::isInstance)
            .map(SdkAutoCloseable.class::cast)
            .map(provider -> cleaner.register(this, provider::close));
  }

  private static AwsSessionCredentials asSessionCredentials(Credentials credentials) {
    return AwsSessionCredentials.create(
        credentials.accessKeyId(), credentials.secretAccessKey(), credentials.sessionToken());
  }

  /**
   * Returns a set of temporary credentials for the configured Role.
   *
   * <p>Credentials should be valid for at least 60 seconds from when provideCredentials is invoked.
   */
  public AwsSessionCredentials resolveCredentials() {
    var credentials = credentialsProvider.resolveCredentials();
    if (credentials instanceof AwsSessionCredentials) {
      return (AwsSessionCredentials) credentials;
    }
    return fallback.get();
  }

  @Override
  public void close() {
    cleanable.ifPresent(Cleanable::clean);
  }
}
