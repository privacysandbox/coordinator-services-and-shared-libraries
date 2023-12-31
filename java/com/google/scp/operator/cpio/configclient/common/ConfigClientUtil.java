/*
 * Copyright 2023 Google LLC
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

import java.time.Duration;

/** Contains utilities for configclient. */
public final class ConfigClientUtil {

  /** Initial interval before the first retry. */
  public static final Duration COORDINATOR_HTTPCLIENT_RETRY_INITIAL_INTERVAL =
      Duration.ofSeconds(5);

  /** Multiplier to increase interval after the first retry. */
  public static final double COORDINATOR_HTTPCLIENT_RETRY_MULTIPLIER = 3.0;

  /** Maximum number of attempts to make. */
  public static final int COORDINATOR_HTTPCLIENT_MAX_ATTEMPTS = 6;

  private ConfigClientUtil() {}
}
