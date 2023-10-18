/*
 * Copyright 2023 Google LLC
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

package com.google.scp.coordinator.pbsprober.service.aws;

import static com.google.scp.coordinator.pbsprober.Annotations.CoordinatorARoleArn;
import static com.google.scp.coordinator.pbsprober.Annotations.CoordinatorBRoleArn;

import com.google.common.base.Strings;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorARegionBinding;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBRegionBinding;
import software.amazon.awssdk.regions.Region;

/** Module to provide environment variable specific to AWS using System.getEnv */
public final class AwsSystemEnvironmentVariableModule extends AbstractModule {
  private static final String COORDINATOR_A_ROLE_ARN = "coordinator_a_role_arn";
  private static final String COORDINATOR_A_REGION = "coordinator_a_region";
  private static final String COORDINATOR_B_ROLE_ARN = "coordinator_b_role_arn";
  private static final String COORDINATOR_B_REGION = "coordinator_b_region";

  @Provides
  @CoordinatorARoleArn
  String provideCoordinatorARoleArn() {
    return getEnvironmentVariable(COORDINATOR_A_ROLE_ARN);
  }

  @Provides
  @CoordinatorARegionBinding
  Region provideCoordinatorARegionBinding() {
    return Region.of(getEnvironmentVariable(COORDINATOR_A_REGION));
  }

  @Provides
  @CoordinatorBRoleArn
  String provideCoordinatorBRoleArn() {
    return getEnvironmentVariable(COORDINATOR_B_ROLE_ARN);
  }

  @Provides
  @CoordinatorBRegionBinding
  Region provideCoordinatorBRegionBinding() {
    return Region.of(getEnvironmentVariable(COORDINATOR_B_REGION));
  }

  private static String getEnvironmentVariable(String key) {
    String result = System.getenv(key);
    if (Strings.isNullOrEmpty(result)) {
      throw new RuntimeException(key + " must be set");
    }
    return result;
  }
}
