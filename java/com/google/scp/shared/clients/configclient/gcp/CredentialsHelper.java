package com.google.scp.shared.clients.configclient.gcp;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.auth.oauth2.GoogleCredentials;
import com.google.scp.shared.mapper.GuavaObjectMapper;
import java.io.ByteArrayInputStream;
import java.io.IOException;

/** Helper class to generate Google credentials. */
public final class CredentialsHelper {
  private static final ObjectMapper mapper = new GuavaObjectMapper();

  /**
   * Provides credentials which can be used by TEE to impersonate Service Account to access
   * protected resources.
   */
  public static GoogleCredentials getAttestedCredentials(
      String wipProvider, String serviceAccountToImpersonate) throws IOException {
    String credentialConfig = getCredentialConfig(wipProvider, serviceAccountToImpersonate);
    return GoogleCredentials.fromStream(new ByteArrayInputStream(credentialConfig.getBytes()));
  }

  private static String getCredentialConfig(String wipProvider, String serviceAccountToImpersonate)
      throws JsonProcessingException {
    CredentialConfig credentialConfig =
        CredentialConfig.builder()
            .type("external_account")
            .audience(String.format("//iam.googleapis.com/%s", wipProvider))
            .credentialSource(
                CredentialSource.builder()
                    .file("/run/container_launcher/attestation_verifier_claims_token")
                    .build())
            .serviceAccountImpersonationUrl(
                String.format(
                    "https://iamcredentials.googleapis.com/v1/projects/-/serviceAccounts/%s:generateAccessToken",
                    serviceAccountToImpersonate))
            .subjectTokenType("urn:ietf:params:oauth:token-type:jwt")
            .tokenUrl("https://sts.googleapis.com/v1/token")
            .build();

    return mapper.writeValueAsString(credentialConfig);
  }
}
