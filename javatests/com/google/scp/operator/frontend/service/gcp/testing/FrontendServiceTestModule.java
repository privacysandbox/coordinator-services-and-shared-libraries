package com.google.scp.operator.frontend.service.gcp.testing;

import static com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueueTestModule.PROJECT_ID;
import static com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueueTestModule.SUBSCRIPTION_ID;
import static com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueueTestModule.TOPIC_ID;
import static com.google.scp.shared.gcp.Constants.GCP_TEST_PROJECT_ID;
import static com.google.scp.shared.gcp.Constants.SPANNER_TEST_DB_NAME;
import static com.google.scp.shared.gcp.Constants.SPANNER_TEST_INSTANCE_ID;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.api.gax.core.NoCredentialsProvider;
import com.google.api.gax.grpc.GrpcTransportChannel;
import com.google.api.gax.rpc.FixedTransportChannelProvider;
import com.google.api.gax.rpc.TransportChannelProvider;
import com.google.cloud.NoCredentials;
import com.google.cloud.pubsub.v1.Publisher;
import com.google.cloud.pubsub.v1.stub.GrpcSubscriberStub;
import com.google.cloud.pubsub.v1.stub.SubscriberStub;
import com.google.cloud.pubsub.v1.stub.SubscriberStubSettings;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.DatabaseId;
import com.google.cloud.spanner.InstanceId;
import com.google.cloud.spanner.SpannerOptions;
import com.google.common.base.Converter;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.inject.TypeLiteral;
import com.google.pubsub.v1.ProjectSubscriptionName;
import com.google.pubsub.v1.TopicName;
import com.google.scp.operator.frontend.service.FrontendService;
import com.google.scp.operator.frontend.service.FrontendServiceImpl;
import com.google.scp.operator.frontend.service.converter.CreateJobRequestToRequestInfoConverter;
import com.google.scp.operator.frontend.service.converter.ErrorCountConverter;
import com.google.scp.operator.frontend.service.converter.ErrorSummaryConverter;
import com.google.scp.operator.frontend.service.converter.GetJobResponseConverter;
import com.google.scp.operator.frontend.service.converter.JobStatusConverter;
import com.google.scp.operator.frontend.service.converter.ResultInfoConverter;
import com.google.scp.operator.frontend.service.gcp.GcpFrontendServiceModule.FrontendServiceVersionBinding;
import com.google.scp.operator.frontend.tasks.gcp.GcpTasksModule;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.frontend.api.v1.ErrorCountProto;
import com.google.scp.operator.protos.frontend.api.v1.ErrorSummaryProto;
import com.google.scp.operator.protos.frontend.api.v1.GetJobResponseProto;
import com.google.scp.operator.protos.frontend.api.v1.JobStatusProto;
import com.google.scp.operator.protos.frontend.api.v1.ResultInfoProto;
import com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueMessageLeaseSeconds;
import com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueue;
import com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueue.JobQueuePubSubSubscriptionName;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobMetadataDbClient;
import com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerMetadataDb.MetadataDbSpannerTtlDays;
import com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerMetadataDbConfig;
import com.google.scp.shared.mapper.TimeObjectMapper;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import java.io.IOException;
import java.util.concurrent.ExecutionException;

/** Module for frontend service integration test */
public class FrontendServiceTestModule extends AbstractModule {
  private static final SpannerMetadataDbConfig DB_CONFIG =
      SpannerMetadataDbConfig.builder()
          .setGcpProjectId(GCP_TEST_PROJECT_ID)
          .setSpannerInstanceId(SPANNER_TEST_INSTANCE_ID)
          .setSpannerDbName(SPANNER_TEST_DB_NAME)
          .build();

  @Provides
  @JobQueueMessageLeaseSeconds
  int provideMessageLeaseSeconds() {
    return 10;
  }

  @Provides
  @JobQueuePubSubSubscriptionName
  String providePubSubSubscriptionName() {
    return ProjectSubscriptionName.format(PROJECT_ID, SUBSCRIPTION_ID);
  }

  @Provides
  @Singleton
  public TransportChannelProvider provideChannelProvider() {
    String hostport = System.getenv("PUBSUB_EMULATOR_HOST");
    ManagedChannel channel = ManagedChannelBuilder.forTarget(hostport).usePlaintext().build();
    return FixedTransportChannelProvider.create(GrpcTransportChannel.create(channel));
  }

  @Provides
  @Singleton
  public Publisher providePublisher(TransportChannelProvider channelProvider) throws IOException {
    NoCredentialsProvider credentialsProvider = NoCredentialsProvider.create();

    return Publisher.newBuilder(TopicName.of(PROJECT_ID, TOPIC_ID))
        .setChannelProvider(channelProvider)
        .setCredentialsProvider(credentialsProvider)
        .build();
  }

  @Provides
  SubscriberStub provideSubscriber(TransportChannelProvider channelProvider) throws IOException {

    NoCredentialsProvider credentialsProvider = NoCredentialsProvider.create();

    SubscriberStubSettings subscriberStubSettings =
        SubscriberStubSettings.newBuilder()
            .setTransportChannelProvider(channelProvider)
            .setCredentialsProvider(credentialsProvider)
            .build();

    return GrpcSubscriberStub.create(subscriberStubSettings);
  }

  @Provides
  @Singleton
  @JobMetadataDbClient
  public DatabaseClient getDatabaseClient() throws ExecutionException, InterruptedException {
    SpannerOptions options =
        SpannerOptions.newBuilder()
            .setEmulatorHost(System.getenv("SPANNER_EMULATOR_HOST"))
            .setCredentials(NoCredentials.getInstance())
            .setProjectId(DB_CONFIG.gcpProjectId())
            .build();
    InstanceId instanceId = InstanceId.of(DB_CONFIG.gcpProjectId(), DB_CONFIG.spannerInstanceId());
    DatabaseId databaseId = DatabaseId.of(instanceId, DB_CONFIG.spannerDbName());
    return options.getService().getDatabaseClient(databaseId);
  }

  @Provides
  @MetadataDbSpannerTtlDays
  Integer provideMetadataDbSpannerTtlDays() {
    return 1;
  }

  /** Configures injected dependencies for this module. */
  @Override
  protected void configure() {
    // Service layer bindings
    bind(FrontendService.class).to(FrontendServiceImpl.class);
    bind(new TypeLiteral<Converter<JobMetadata, GetJobResponseProto.GetJobResponse>>() {})
        .to(GetJobResponseConverter.class);
    bind(new TypeLiteral<Converter<CreateJobRequest, RequestInfo>>() {})
        .to(CreateJobRequestToRequestInfoConverter.class);
    bind(new TypeLiteral<
            Converter<
                com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo,
                ResultInfoProto.ResultInfo>>() {})
        .to(ResultInfoConverter.class);
    bind(new TypeLiteral<
            Converter<
                com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary,
                ErrorSummaryProto.ErrorSummary>>() {})
        .to(ErrorSummaryConverter.class);
    bind(new TypeLiteral<
            Converter<
                com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus,
                JobStatusProto.JobStatus>>() {})
        .to(JobStatusConverter.class);
    bind(new TypeLiteral<
            Converter<
                com.google.scp.operator.protos.shared.backend.ErrorCountProto.ErrorCount,
                ErrorCountProto.ErrorCount>>() {})
        .to(ErrorCountConverter.class);
    bind(ObjectMapper.class).to(TimeObjectMapper.class);
    bind(int.class).annotatedWith(FrontendServiceVersionBinding.class).toInstance(1);

    // Business layer bindings
    install(new GcpTasksModule());

    bind(JobMetadataDb.class).to(SpannerMetadataDb.class);
    bind(JobQueue.class).to(PubSubJobQueue.class);
  }
}
