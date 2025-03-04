/*
 * Copyright 2024 Google LLC
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

package com.google.scp.operator.cpio.configclient.common;

import com.google.auto.value.AutoValue;
import java.util.Optional;

/**
 * Holds configuration parameters for {@code GcpOperatorClientConfigModule} and {@code
 * Aws2GcpOperatorClientConfigModule}.
 */
@AutoValue
public abstract class OperatorClientConfig {

  /** Returns an instance of the {@code OperatorClientConfig.Builder} class. */
  public static OperatorClientConfig.Builder builder() {
    return new AutoValue_OperatorClientConfig.Builder();
  }

  /** CoordinatorA Workload Identity Pool Provider. */
  public abstract Optional<String> coordinatorAWipProvider();

  /**
   * CoordinatorA Service Account which TEE can impersonate and has access to protected resources.
   */
  public abstract Optional<String> coordinatorAServiceAccountToImpersonate();

  /** CoordinatorB Workload Identity Pool Provider. */
  public abstract Optional<String> coordinatorBWipProvider();

  /**
   * CoordinatorB Service Account which TEE can impersonate and has access to protected resources.
   */
  public abstract Optional<String> coordinatorBServiceAccountToImpersonate();

  /** Whether to use local, unattested credentials. */
  public abstract boolean useLocalCredentials();

  /** Coordinator A encryption key service url */
  public abstract String coordinatorAEncryptionKeyServiceBaseUrl();

  /** Coordinator B encryption key service url */
  public abstract String coordinatorBEncryptionKeyServiceBaseUrl();

  /** Coordinator A encryption key service cloudfunction url */
  public abstract Optional<String> coordinatorAEncryptionKeyServiceCloudfunctionUrl();

  /** Coordinator B encryption key service cloudfunction url */
  public abstract Optional<String> coordinatorBEncryptionKeyServiceCloudfunctionUrl();

  /** Builder class for the {@code OperatorClientConfig} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Set the CoordinatorA Workload Identity Pool Provider. */
    public abstract Builder setCoordinatorAWipProvider(Optional<String> coordinatorAWipProvider);

    /** Set the CoordinatorA Service Account which can be impersonated by TEE. */
    public abstract Builder setCoordinatorAServiceAccountToImpersonate(
        Optional<String> coordinatorAServiceAccountToImpersonate);

    /** Set the CoordinatorB Workload Identity Pool Provider. */
    public abstract Builder setCoordinatorBWipProvider(Optional<String> coordinatorBWipProvider);

    /** Set the CoordinatorB Service Account which can be impersonated by TEE. */
    public abstract Builder setCoordinatorBServiceAccountToImpersonate(
        Optional<String> coordinatorBServiceAccountToImpersonate);

    /** Set whether to use local, unattested credentials. */
    public abstract Builder setUseLocalCredentials(boolean useLocalCredentials);

    /** Set coordinator A encryption key service base url */
    public abstract Builder setCoordinatorAEncryptionKeyServiceBaseUrl(
        String coordinatorAEncryptionKeyServiceBaseUrl);

    /** Set coordinator B encryption key service base url */
    public abstract Builder setCoordinatorBEncryptionKeyServiceBaseUrl(
        String coordinatorBEncryptionKeyServiceBaseUrl);

    /** Set coordinator A encryption key service base url */
    public abstract Builder setCoordinatorAEncryptionKeyServiceCloudfunctionUrl(
        Optional<String> coordinatorAEncryptionKeyServiceCloudfunctionUrl);

    /** Set coordinator B encryption key service base url */
    public abstract Builder setCoordinatorBEncryptionKeyServiceCloudfunctionUrl(
        Optional<String> coordinatorBEncryptionKeyServiceCloudfunctionUrl);

    /** Uses the builder to construct a new instance of the {@code OperatorClientConfig} class. */
    public abstract OperatorClientConfig build();
  }
}
