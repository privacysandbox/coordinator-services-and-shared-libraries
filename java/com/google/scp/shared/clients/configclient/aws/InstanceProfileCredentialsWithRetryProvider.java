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

package com.google.scp.shared.clients.configclient.aws;

import io.github.resilience4j.retry.Retry;
import software.amazon.awssdk.auth.credentials.AwsCredentials;
import software.amazon.awssdk.auth.credentials.HttpCredentialsProvider;
import software.amazon.awssdk.auth.credentials.InstanceProfileCredentialsProvider;
import software.amazon.awssdk.utils.ToString;

/**
 * Credentials provider implementation that loads credentials from the Amazon EC2 Instance Metadata
 * Service, including a retry policy.
 */
public final class InstanceProfileCredentialsWithRetryProvider implements HttpCredentialsProvider {

  private final Retry retryConfig;
  private final InstanceProfileCredentialsProvider credentialsProvider;

  /**
   * @see #builder()
   */
  private InstanceProfileCredentialsWithRetryProvider(BuilderImpl builder) {
    this.retryConfig = builder.retryConfig;
    this.credentialsProvider =
        InstanceProfileCredentialsProvider.builder()
            .endpoint(builder.endpoint)
            .asyncCredentialUpdateEnabled(builder.asyncCredentialUpdateEnabled)
            .asyncThreadName(builder.asyncThreadName)
            .build();
  }

  /** Create a builder for creating a {@link InstanceProfileCredentialsWithRetryProvider}. */
  public static Builder builder() {
    return new BuilderImpl();
  }

  /**
   * Create a {@link InstanceProfileCredentialsWithRetryProvider} with default values.
   *
   * @return a {@link InstanceProfileCredentialsWithRetryProvider}
   */
  public static InstanceProfileCredentialsWithRetryProvider create() {
    return builder().build();
  }

  @Override
  public AwsCredentials resolveCredentials() {
    // This calls the supplier method, and can retry based on the provided retry configuration
    return Retry.decorateSupplier(retryConfig, credentialsProvider::resolveCredentials).get();
  }

  @Override
  public void close() {
    credentialsProvider.close();
  }

  @Override
  public String toString() {
    return ToString.create("InstanceProfileCredentialsWithRetryProvider");
  }

  /** A builder for creating a custom a {@link InstanceProfileCredentialsWithRetryProvider}. */
  public interface Builder
      extends HttpCredentialsProvider.Builder<
          InstanceProfileCredentialsWithRetryProvider, Builder> {
    /** Set the endpoint to be used by the provider. */
    Builder endpoint(String endpoint);

    /** Set the retry configuration used by the provider. */
    Builder retryConfig(Retry retry);

    /** Build a {@link InstanceProfileCredentialsWithRetryProvider} from the provided config. */
    InstanceProfileCredentialsWithRetryProvider build();
  }

  static final class BuilderImpl implements Builder {
    private String endpoint;
    private Boolean asyncCredentialUpdateEnabled;
    private String asyncThreadName;
    private Retry retryConfig;

    private BuilderImpl() {
      asyncThreadName("instance-profile-credentials-retry-provider");
    }

    @Override
    public Builder endpoint(String endpoint) {
      this.endpoint = endpoint;
      return this;
    }

    public void setEndpoint(String endpoint) {
      endpoint(endpoint);
    }

    @Override
    public Builder asyncCredentialUpdateEnabled(Boolean asyncCredentialUpdateEnabled) {
      this.asyncCredentialUpdateEnabled = asyncCredentialUpdateEnabled;
      return this;
    }

    public void setAsyncCredentialUpdateEnabled(boolean asyncCredentialUpdateEnabled) {
      asyncCredentialUpdateEnabled(asyncCredentialUpdateEnabled);
    }

    @Override
    public Builder asyncThreadName(String asyncThreadName) {
      this.asyncThreadName = asyncThreadName;
      return this;
    }

    public void setAsyncThreadName(String asyncThreadName) {
      asyncThreadName(asyncThreadName);
    }

    @Override
    public Builder retryConfig(Retry retryConfig) {
      this.retryConfig = retryConfig;
      return this;
    }

    public void setRetryConfig(Retry retryConfig) {
      retryConfig(retryConfig);
    }

    @Override
    public InstanceProfileCredentialsWithRetryProvider build() {
      return new InstanceProfileCredentialsWithRetryProvider(this);
    }
  }
}
