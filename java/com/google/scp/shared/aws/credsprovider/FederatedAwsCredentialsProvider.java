package com.google.scp.shared.aws.credsprovider;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.auth.oauth2.IdTokenCredentials;
import com.google.auth.oauth2.IdTokenProvider;
import java.io.IOException;
import java.util.function.Supplier;
import software.amazon.awssdk.auth.credentials.AnonymousCredentialsProvider;
import software.amazon.awssdk.auth.credentials.AwsCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.sts.StsClient;
import software.amazon.awssdk.services.sts.auth.StsAssumeRoleWithWebIdentityCredentialsProvider;
import software.amazon.awssdk.services.sts.model.AssumeRoleWithWebIdentityRequest;

/**
 * An {@link AwsCredentialsProvider} implementation that enables applications running in GCP access
 * to AWS resources. Exchanges GCP credentials for temporary AWS credentials for a specified IAM
 * role through the AWS STS.
 */
public class FederatedAwsCredentialsProvider implements AwsCredentialsProvider {

  private static final String AWS_STS_AUDIENCE = "https://sts.amazonaws.com";
  private static final Region AWS_STS_REGION = Region.US_EAST_1;
  private static final String AWS_STS_SESSION_NAME = "federated-session";

  private AwsCredentialsProvider awsCredentialsProvider;

  public FederatedAwsCredentialsProvider(String targetRoleArn) {
    awsCredentialsProvider =
        StsAssumeRoleWithWebIdentityCredentialsProvider.builder()
            .stsClient(createStsClient())
            .refreshRequest(createAssumeRoleRequestSupplier(targetRoleArn, getGcpIdTokenProvider()))
            .build();
  }

  @Override
  public AwsCredentials resolveCredentials() {
    return awsCredentialsProvider.resolveCredentials();
  }

  private IdTokenProvider getGcpIdTokenProvider() {
    try {
      return (IdTokenProvider) GoogleCredentials.getApplicationDefault();
    } catch (IOException e) {
      throw new RuntimeException("Error getting default GCP token provider.", e);
    }
  }

  private StsClient createStsClient() {
    return StsClient.builder()
        .region(AWS_STS_REGION)
        .credentialsProvider(AnonymousCredentialsProvider.create())
        .build();
  }

  private Supplier<AssumeRoleWithWebIdentityRequest> createAssumeRoleRequestSupplier(
      String targetRoleArn, IdTokenProvider gcpTokenProvider) {
    return () ->
        AssumeRoleWithWebIdentityRequest.builder()
            .webIdentityToken(getGcpIdToken(gcpTokenProvider))
            .roleArn(targetRoleArn)
            .roleSessionName(AWS_STS_SESSION_NAME)
            .build();
  }

  private String getGcpIdToken(IdTokenProvider gcpTokenProvider) {
    try {
      return IdTokenCredentials.newBuilder()
          .setIdTokenProvider(gcpTokenProvider)
          .setTargetAudience(AWS_STS_AUDIENCE)
          .build()
          .refreshAccessToken()
          .getTokenValue();
    } catch (IOException e) {
      throw new RuntimeException("Unable to get GCP ID token.", e);
    }
  }
}
