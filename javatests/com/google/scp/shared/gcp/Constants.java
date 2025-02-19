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

package com.google.scp.shared.gcp;

/** Contains all the constants that are used across all GCP tests. */
public final class Constants {

  private Constants() {}

  public static final String GCP_TEST_PROJECT_ID = "test-project";

  public static final String SPANNER_TEST_DB_NAME = "test-database";

  public static final String SPANNER_TEST_INSTANCE_ID = "test-instance";

  public static final String SPANNER_COORD_B_TEST_INSTANCE_ID = "test-instance-b";
}
