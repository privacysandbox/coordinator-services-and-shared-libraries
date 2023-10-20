package com.google.scp.shared.testutils.gcp;

import com.google.api.gax.core.NoCredentialsProvider;
import com.google.api.gax.grpc.GrpcTransportChannel;
import com.google.api.gax.rpc.FixedTransportChannelProvider;
import com.google.api.gax.rpc.TransportChannelProvider;
import com.google.cloud.pubsub.v1.Publisher;
import com.google.cloud.pubsub.v1.stub.SubscriberStub;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import java.io.IOException;
import org.testcontainers.utility.DockerImageName;

public class PubSubEmulatorContainerTestModule extends AbstractModule {

  private final String projectId;
  private final String topicId;
  private final String subscriptionId;
  private final boolean startImmediately;

  public PubSubEmulatorContainerTestModule(
      String projectId, String topicId, String subscriptionId) {
    this(projectId, topicId, subscriptionId, false);
  }

  public PubSubEmulatorContainerTestModule(
      String projectId, String topicId, String subscriptionId, boolean startImmediately) {
    this.projectId = projectId;
    this.topicId = topicId;
    this.subscriptionId = subscriptionId;
    this.startImmediately = startImmediately;
  }

  @Provides
  @Singleton
  public PubSubEmulatorContainer providePubSubEmulator() {
    return new PubSubEmulatorContainer(
        DockerImageName.parse("gcr.io/google.com/cloudsdktool/cloud-sdk:316.0.0-emulators"),
        startImmediately);
  }

  @Provides
  @Singleton
  public TransportChannelProvider provideChannelProvider(PubSubEmulatorContainer emulator) {
    String hostport = emulator.getEmulatorEndpoint();
    ManagedChannel channel = ManagedChannelBuilder.forTarget(hostport).usePlaintext().build();
    return FixedTransportChannelProvider.create(GrpcTransportChannel.create(channel));
  }

  @Provides
  @Singleton
  public Publisher providePublisher(TransportChannelProvider channelProvider) throws IOException {
    NoCredentialsProvider credentialsProvider = NoCredentialsProvider.create();

    PubSubContainerUtil.createTopic(projectId, topicId, channelProvider, credentialsProvider);
    PubSubContainerUtil.createSubscription(
        projectId, topicId, subscriptionId, channelProvider, credentialsProvider);

    return PubSubContainerUtil.createPublisher(
        projectId, topicId, channelProvider, credentialsProvider);
  }

  @Provides
  SubscriberStub provideSubscriber(TransportChannelProvider channelProvider) throws IOException {
    return PubSubContainerUtil.createSubscriptionStub(
        channelProvider, NoCredentialsProvider.create());
  }
}
