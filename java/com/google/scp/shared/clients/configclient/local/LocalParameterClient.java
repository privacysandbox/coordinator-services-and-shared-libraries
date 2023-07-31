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

package com.google.scp.shared.clients.configclient.local;

import static com.google.scp.shared.clients.configclient.local.Annotations.ParameterValues;

import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.clients.configclient.ParameterClient;
import java.util.Optional;
import javax.inject.Inject;

/**
 * Parameter client implementation for getting parameters from cli args. This helps with running the
 * code locally without changing parameter values in the cloud.
 */
public final class LocalParameterClient implements ParameterClient {

  private final ImmutableMap<String, String> parameterValues;

  /** Creates a new instance of the {@code LocalParameterClient} class. */
  @Inject
  LocalParameterClient(@ParameterValues ImmutableMap<String, String> parameterValues) {
    this.parameterValues = parameterValues;
  }

  /**
   * Gets a parameter from the local environment flags.
   *
   * <p>The raw parameter name will be used to query the parameter mapping, with no prefixes or
   * environments added.
   *
   * @return an {@link Optional} of {@link String} for parameter value
   */
  @Override
  public Optional<String> getParameter(String param) throws ParameterClientException {
    return Optional.ofNullable(parameterValues.get(param));
  }

  /**
   * Gets a parameter from the local environment flags.
   *
   * <p>The raw parameter name will be used to query the parameter mapping, with no prefixes or
   * environments added. `paramPrefix` and `includeEnvironmentParam` flags are ignored in local
   * mode.
   *
   * @return an {@link Optional} of {@link String} for parameter value
   */
  @Override
  public Optional<String> getParameter(
      String param, Optional<String> paramPrefix, boolean includeEnvironmentParam)
      throws ParameterClientException {
    return getParameter(param);
  }

  @Override
  public Optional<String> getEnvironmentName() {
    return Optional.of("local");
  }
}
