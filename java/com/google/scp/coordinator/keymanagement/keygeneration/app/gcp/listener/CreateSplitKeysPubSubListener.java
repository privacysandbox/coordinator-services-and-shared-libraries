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

package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener;

import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationKeyCount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationTtlInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationValidityInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.Annotations.SubscriptionId;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTask;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpProjectId;

/** Listens for Pub/Sub message and generates keys when message is received. */
public final class CreateSplitKeysPubSubListener extends PubSubListener {

  private final CreateSplitKeyTask task;

  @Inject
  public CreateSplitKeysPubSubListener(
      PubSubListenerConfig config,
      CreateSplitKeyTask task,
      @GcpProjectId String projectId,
      @SubscriptionId String subscriptionId,
      @KeyGenerationKeyCount Integer numberOfKeysToCreate,
      @KeyGenerationValidityInDays Integer keysValidityInDays,
      @KeyGenerationTtlInDays Integer ttlInDays) {
    super(config, projectId, subscriptionId, numberOfKeysToCreate, keysValidityInDays, ttlInDays);
    this.task = task;
  }

  /** Executes task that will generate split keys (if needed) after Pub/Sub message is received. */
  @Override
  protected void createKeys(int numberOfKeysToCreate, int keysValidityInDays, int ttlInDays)
      throws ServiceException {
    task.create(
        /* numDesiredKeys= */ numberOfKeysToCreate,
        /* validityInDays= */ keysValidityInDays,
        /* ttlInDays= */ ttlInDays);
  }
}
