package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import com.google.crypto.tink.Aead;
import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.shared.util.GcpAeadProvider;
import java.util.Map;

class AwsEncryptionAeadModule extends AbstractModule {

  private static final String AWS_KMS_URI_ENV_VAR = "AWS_KMS_URI";
  private static final String AWS_KMS_ROLE_ARN_ENV_VAR = "AWS_KMS_ROLE_ARN";
  Map<String, String> env;

  AwsEncryptionAeadModule(Map<String, String> env) {
    this.env = env;
  }

  @Override
  protected void configure() {
    String awsKmsUri = env.get(AWS_KMS_URI_ENV_VAR);
    String awsKmsRoleArn = env.get(AWS_KMS_ROLE_ARN_ENV_VAR);
    bind(Aead.class)
        .annotatedWith(EncryptionAead.class)
        .toInstance(GcpAeadProvider.getAwsAead(awsKmsUri, awsKmsRoleArn));
    bind(String.class).annotatedWith(EncryptionKeyUri.class).toInstance(awsKmsUri);
  }
}
