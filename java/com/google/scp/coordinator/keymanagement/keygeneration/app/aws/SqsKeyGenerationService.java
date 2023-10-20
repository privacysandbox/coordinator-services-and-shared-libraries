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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws;

import com.google.common.util.concurrent.AbstractExecutionThreadService;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.model.KeyGenerationQueueItem;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationKeyCount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationTtlInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationValidityInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTask;
import java.util.Optional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Guava service for repeatedly pulling from the SQS queue and generating a key */
public final class SqsKeyGenerationService extends AbstractExecutionThreadService {

  private static final Long THREAD_SLEEP_TIME_MILLIS = 5000L;
  private static final Logger logger = LoggerFactory.getLogger(SqsKeyGenerationService.class);

  private final SqsKeyGenerationQueue sqsKeyGenerationQueue;
  private final CreateSplitKeyTask createSplitKeyTask;
  private final int numDesiredKeys;
  private final int validityInDays;
  private final int ttlInDays;

  // Tracks whether the service should be pulling more jobs. Once the shutdown of the service is
  // initiated, this is switched to false.
  private volatile boolean moreNewRequests;

  @Inject
  public SqsKeyGenerationService(
      SqsKeyGenerationQueue sqsKeyGenerationQueue,
      CreateSplitKeyTask createSplitKeyTask,
      @KeyGenerationKeyCount int numDesiredKeys,
      @KeyGenerationValidityInDays int validityInDays,
      @KeyGenerationTtlInDays int ttlInDays) {
    this.sqsKeyGenerationQueue = sqsKeyGenerationQueue;
    this.createSplitKeyTask = createSplitKeyTask;
    this.numDesiredKeys = numDesiredKeys;
    this.ttlInDays = ttlInDays;
    this.validityInDays = validityInDays;
    this.moreNewRequests = true;
  }

  @Override
  protected void run() {
    logger.info("Key generation service started");
    while (moreNewRequests) {

      try {
        Optional<KeyGenerationQueueItem> item = sqsKeyGenerationQueue.pullQueueItem();

        if (item.isEmpty()) {
          Thread.sleep(THREAD_SLEEP_TIME_MILLIS);
        } else {
          logger.info("Key generation message pulled");
          createSplitKeyTask.create(numDesiredKeys, validityInDays, ttlInDays);
          sqsKeyGenerationQueue.acknowledgeKeyGenerationCompletion(item.get());
        }

      } catch (Exception e) {
        // Simply log exceptions that occur so that the worker doesn't crash
        logger.error("Exception occurred in key generation worker", e);
      }
    }
  }

  @Override
  protected void triggerShutdown() {
    moreNewRequests = false;
  }
}
