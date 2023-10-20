package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener;

import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationKeyCount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationTtlInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationValidityInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.Annotations.SubscriptionId;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateKeysTask;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpProjectId;

/**
 * Listens for PubSub message and generates keys when message is received.
 *
 * @deprecated Single-party key features are deprecated. Pending removal b/282204533.
 */
@Deprecated
public final class CreateKeysPubSubListener extends PubSubListener {

  private final CreateKeysTask task;

  @Inject
  public CreateKeysPubSubListener(
      PubSubListenerConfig config,
      CreateKeysTask task,
      @GcpProjectId String projectId,
      @SubscriptionId String subscriptionId,
      @KeyGenerationKeyCount Integer numberOfKeysToCreate,
      @KeyGenerationValidityInDays Integer keysValidityInDays,
      @KeyGenerationTtlInDays Integer ttlInDays) {
    super(config, projectId, subscriptionId, numberOfKeysToCreate, keysValidityInDays, ttlInDays);
    this.task = task;
  }

  /** Executes task that will generate keys (if needed) after PubSub message is received. */
  @Override
  protected void createKeys(int numberOfKeysToCreate, int keysValidityInDays, int ttlInDays)
      throws ServiceException {
    task.replaceExpiringKeys(
        /* numDesiredKeys= */ numberOfKeysToCreate,
        /* validityInDays= */ keysValidityInDays,
        /* ttlInDays= */ ttlInDays);
  }
}
