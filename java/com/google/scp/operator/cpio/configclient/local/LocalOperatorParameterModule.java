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

package com.google.scp.operator.cpio.configclient.local;

import static com.google.scp.operator.cpio.configclient.local.Annotations.CoordinatorKmsArnParameter;
import static com.google.scp.operator.cpio.configclient.local.Annotations.DdbJobMetadataTableNameParameter;
import static com.google.scp.operator.cpio.configclient.local.Annotations.MaxJobNumAttemptsParameter;
import static com.google.scp.operator.cpio.configclient.local.Annotations.MaxJobProcessingTimeSecondsParameter;
import static com.google.scp.operator.cpio.configclient.local.Annotations.ScaleInHookParameter;
import static com.google.scp.operator.cpio.configclient.local.Annotations.SqsJobQueueUrlParameter;
import static com.google.scp.shared.clients.configclient.local.Annotations.ParameterValues;

import com.google.common.collect.ImmutableMap;
import com.google.inject.Key;
import com.google.inject.Provides;
import com.google.inject.multibindings.OptionalBinder;
import com.google.scp.operator.cpio.configclient.local.Annotations.CoordinatorARoleArn;
import com.google.scp.operator.cpio.configclient.local.Annotations.CoordinatorBRoleArn;
import com.google.scp.operator.cpio.configclient.local.Annotations.WorkerAutoscalingGroup;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterModule;
import com.google.scp.shared.clients.configclient.local.LocalParameterClient;
import com.google.scp.shared.clients.configclient.model.WorkerParameter;

/** Guice module for binding the local parameter client functionality specific to operators */
public final class LocalOperatorParameterModule extends ParameterModule {

  @Override
  public Class<? extends ParameterClient> getParameterClientImpl() {
    return LocalParameterClient.class;
  }

  @Provides
  @ParameterValues
  ImmutableMap<String, String> provideParameterValues(
      @SqsJobQueueUrlParameter String sqsUrl,
      @DdbJobMetadataTableNameParameter String ddbTableName,
      @MaxJobNumAttemptsParameter String maxJobNumAttempts,
      @MaxJobProcessingTimeSecondsParameter String maxJobProcessingTimeSeconds,
      @CoordinatorARoleArn String coordinatorARoleArn,
      @CoordinatorBRoleArn String coordinatorBRoleArn,
      @CoordinatorKmsArnParameter String coordinatorKmsArn,
      @ScaleInHookParameter String scaleInHookParameter,
      @WorkerAutoscalingGroup String workerAutoscalingGroup) {
    return ImmutableMap.<String, String>builder()
        .put(WorkerParameter.JOB_QUEUE.name(), sqsUrl)
        .put(WorkerParameter.JOB_METADATA_DB.name(), ddbTableName)
        .put(WorkerParameter.MAX_JOB_NUM_ATTEMPTS.name(), maxJobNumAttempts)
        .put(WorkerParameter.MAX_JOB_PROCESSING_TIME_SECONDS.name(), maxJobProcessingTimeSeconds)
        .put(WorkerParameter.COORDINATOR_A_ROLE.name(), coordinatorARoleArn)
        .put(WorkerParameter.COORDINATOR_B_ROLE.name(), coordinatorBRoleArn)
        .put(WorkerParameter.COORDINATOR_KMS_ARN.name(), coordinatorKmsArn)
        .put(WorkerParameter.SCALE_IN_HOOK.name(), scaleInHookParameter)
        .put(WorkerParameter.WORKER_AUTOSCALING_GROUP.name(), workerAutoscalingGroup)
        .build();
  }

  @Override
  public void customConfigure() {
    OptionalBinder.newOptionalBinder(binder(), Key.get(String.class, WorkerAutoscalingGroup.class))
        .setDefault()
        .toInstance("fakeGroup");
  }
}
