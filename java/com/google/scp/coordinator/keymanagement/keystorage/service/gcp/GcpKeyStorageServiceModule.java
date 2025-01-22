package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import com.google.crypto.tink.Aead;
import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKekUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKeyAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DecryptionAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.CreateKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.SignDataKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.gcp.GcpCreateKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.gcp.GcpSignDataKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbConfig;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbModule;
import com.google.scp.coordinator.keymanagement.shared.util.GcpAeadProvider;
import java.util.Map;
import java.util.Optional;

/**
 * Defines dependencies for GCP implementation of KeyStorageService. It is intended to be used with
 * the CreateKeyHttpFunction.
 */
public class GcpKeyStorageServiceModule extends AbstractModule {
  private static final String PROJECT_ID_ENV_VAR = "PROJECT_ID";
  private static final String READ_STALENESS_SEC_ENV_VAR = "READ_STALENESS_SEC";
  private static final String SPANNER_INSTANCE_ENV_VAR = "SPANNER_INSTANCE";
  private static final String SPANNER_DATABASE_ENV_VAR = "SPANNER_DATABASE";
  private static final String SPANNER_ENDPOINT_ENV_VAR = "SPANNER_ENDPOINT";
  private static final String GCP_KMS_URI_ENV_VAR = "GCP_KMS_URI";
  private static final String AWS_XC_ENABLED_ENV_VAR = "AWS_XC_ENABLED";

  /** Returns ReadStalenessSeconds as Integer from environment variables. Default value of 15 */
  private Integer getReadStalenessSeconds(Map<String, String> env) {
    return Integer.valueOf(env.getOrDefault(READ_STALENESS_SEC_ENV_VAR, "15"));
  }

  private Boolean isAwsXcEnabled(Map<String, String> env) {
    return env.getOrDefault(AWS_XC_ENABLED_ENV_VAR, "false").equalsIgnoreCase("true");
  }

  @Override
  protected void configure() {
    Map<String, String> env = System.getenv();
    String gcpKmsUri = env.getOrDefault(GCP_KMS_URI_ENV_VAR, "unknown_gcp_uri");
    String projectId = env.getOrDefault(PROJECT_ID_ENV_VAR, "adhcloud-tp2");
    String spannerInstanceId = env.getOrDefault(SPANNER_INSTANCE_ENV_VAR, "keydbinstance");
    String spannerDatabaseId = env.getOrDefault(SPANNER_DATABASE_ENV_VAR, "keydb");
    String spannerEndpoint = env.get(SPANNER_ENDPOINT_ENV_VAR);

    // Business layer bindings
    bind(CreateKeyTask.class).to(GcpCreateKeyTask.class);
    bind(SignDataKeyTask.class).to(GcpSignDataKeyTask.class);

    Aead gcpAead = GcpAeadProvider.getGcpAead(gcpKmsUri, Optional.empty());
    bind(Aead.class).annotatedWith(DecryptionAead.class).toInstance(gcpAead);
    if (isAwsXcEnabled(env)) {
      install(new AwsEncryptionAeadModule(env));
    } else {
      install(new GcpEncryptionAeadModule(gcpKmsUri));
    }

    // TODO: refactor so that GCP does not need these. Placeholder values since they are not used
    bind(String.class).annotatedWith(CoordinatorKekUri.class).toInstance("");
    bind(Aead.class).annotatedWith(CoordinatorKeyAead.class).toInstance(gcpAead);

    // Data layer bindings
    SpannerKeyDbConfig config =
        SpannerKeyDbConfig.builder()
            .setGcpProjectId(projectId)
            .setSpannerInstanceId(spannerInstanceId)
            .setSpannerDbName(spannerDatabaseId)
            .setReadStalenessSeconds(getReadStalenessSeconds(env))
            .setEndpointUrl(Optional.ofNullable(spannerEndpoint))
            .build();
    bind(SpannerKeyDbConfig.class).toInstance(config);
    install(new SpannerKeyDbModule());
  }
}
