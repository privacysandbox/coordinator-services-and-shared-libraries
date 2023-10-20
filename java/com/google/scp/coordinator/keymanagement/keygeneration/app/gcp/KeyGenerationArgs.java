package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp;

import com.beust.jcommander.Parameter;
import com.google.common.annotations.Beta;
import java.util.Optional;

/** Runtime arguments for Key Generation Application. */
public final class KeyGenerationArgs {

  @Parameter(
      names = "--kms-key-uri",
      description = "GCP KMS key Uri used to encrypt the private key split.")
  private String kmsKeyUri = "";

  @Parameter(names = "--spanner-instance-id", description = "Spanner instance id.")
  private String spannerInstanceId = "";

  @Parameter(names = "--spanner-database-id", description = "Spanner database id.")
  private String databaseId = "";

  @Parameter(
      names = "--spanner-endpoint",
      description =
          "GCP Spanner endpoint URL to override the default value. Values that do not start with"
              + " \"https://\" are assumed to be emulators for testing. Empty value is ignored.")
  private String spannerEndpoint = "";

  @Parameter(
      names = "--pubsub-endpoint",
      description =
          "GCP pubsub endpoint URL to override the default value. Empty value is ignored.")
  private String pubSubEndpoint = "";

  @Parameter(
      names = "--project-id",
      description = "Project Id where pub-sub, database and kms keys are defined.")
  private String projectId = "";

  @Parameter(
      names = "--subscription-id",
      description =
          "Subscription id of pull subscription associated with pub sub topic which publishes key"
              + " generation message.")
  private String subscriptionId = "";

  @Parameter(
      names = "--number-of-keys-to-create",
      description = "Number of keys to create when event is triggered.")
  private int numberOfKeysToCreate = 5;

  @Parameter(
      names = "--keys-validity-in-days",
      description = "Number of days public keys are valid for use.")
  private int keysValidityInDays = 8;

  @Parameter(
      names = "--ttl-in-days",
      description = "Number of days after creation before purging a key from the database.")
  private int ttlInDays = 365;

  @Parameter(names = "--multiparty", description = "Set to true for multiparty key generation.")
  private boolean multiparty = false;

  @Parameter(
      names = "--peer-coordinator-kms-key-uri",
      description =
          "GCP KMS key Uri used to encrypt the private key split for Coordinator B. Key is provided"
              + " by Coordinator B.")
  private String peerCoordinatorKmsKeyUri = "";

  @Parameter(
      names = "--peer-coordinator-wip-provider",
      description = "Workload identity pool provider id from Coordinator B.")
  private String peerCoordinatorWipProvider = "";

  @Parameter(
      names = "--peer-coordinator-service-account",
      description = "Coordinator service account used to access KMS Key.")
  private String peerCoordinatorServiceAccount = "";

  @Parameter(
      names = "--key_storage_service_base_url",
      description =
          "Base URL for Coordinator B HTTP services (used for key exchange service and key storage"
              + " service)")
  private String keyStorageServiceBaseUrl = "";

  @Parameter(
      names = "--key_storage_service_cloudfunction_url",
      description =
          "Cloudfunction url for Coordinator B HTTP services "
              + "(used as audience for gcp OIDC authentication only) "
              + "This is temporary and will be replaced by key storage service url in the future. ")
  private String keyStorageServiceCloudfunctionUrl = "";

  @Parameter(names = "--key_id_type", description = "Key ID Type")
  private String keyIdType = "";

  /** TODO: delete once local KMS has been implemented and can replace this stopgap feature. */
  @Beta
  @Parameter(
      names = "--test-encoded-keyset-handle",
      description =
          "The encoded keyset handle to use for encryption."
              + " This is an optional parameter to use to override the default behavior.")
  private String testEncodedKeysetHandle = "";

  /** TODO: delete once local KMS has been implemented and can replace this stopgap feature. */
  @Beta
  @Parameter(
      names = "--test-peer-coordinator-encoded-keyset-handle",
      description =
          "The encoded keyset handle for the peer coordinator to use for encryption."
              + " This is an optional parameter to use to override the default behavior.")
  private String testPeerCoordinatorEncodedKeysetHandle = "";

  @Beta
  @Parameter(
      names = "--test-use-default-parameters-on-gcp",
      description = "Whether or not the default parameters should be used on GCP.")
  private boolean testUseDefaultParametersOnGcp = false;

  public String getKmsKeyUri() {
    return kmsKeyUri;
  }

  public String getSpannerInstanceId() {
    return spannerInstanceId;
  }

  public String getDatabaseId() {
    return databaseId;
  }

  public String getProjectId() {
    return projectId;
  }

  public String getSubscriptionId() {
    return subscriptionId;
  }

  public int getNumberOfKeysToCreate() {
    return numberOfKeysToCreate;
  }

  public int getKeysValidityInDays() {
    return keysValidityInDays;
  }

  public int getTtlInDays() {
    return ttlInDays;
  }

  public String getPeerCoordinatorKmsKeyUri() {
    return peerCoordinatorKmsKeyUri;
  }

  public String getKeyStorageServiceBaseUrl() {
    return keyStorageServiceBaseUrl;
  }

  public Optional<String> getKeyStorageServiceCloudfunctionUrl() {
    return Optional.ofNullable(keyStorageServiceCloudfunctionUrl).filter(id -> !id.isEmpty());
  }

  public boolean isMultiparty() {
    return multiparty;
  }

  public String getPeerCoordinatorWipProvider() {
    return peerCoordinatorWipProvider;
  }

  public String getPeerCoordinatorServiceAccount() {
    return peerCoordinatorServiceAccount;
  }

  public Optional<String> getSpannerEndpoint() {
    return Optional.ofNullable(spannerEndpoint).filter(endpoint -> !endpoint.isEmpty());
  }

  public Optional<String> getPubSubEndpoint() {
    return Optional.ofNullable(pubSubEndpoint).filter(endpoint -> !endpoint.isEmpty());
  }

  public Optional<String> getKeyIdType() {
    return Optional.of(keyIdType).filter(type -> !type.isEmpty());
  }

  /** TODO: delete once local KMS has been implemented and can replace this stopgap feature. */
  @Beta
  public Optional<String> getTestEncodedKeysetHandle() {
    return Optional.ofNullable(testEncodedKeysetHandle).filter(s -> !s.isEmpty());
  }

  /** TODO: delete once local KMS has been implemented and can replace this stopgap feature. */
  @Beta
  public Optional<String> getTestPeerCoordinatorEncodedKeysetHandle() {
    return Optional.ofNullable(testPeerCoordinatorEncodedKeysetHandle).filter(s -> !s.isEmpty());
  }

  @Beta
  public boolean getTestUseDefaultParametersOnGcp() {
    return testUseDefaultParametersOnGcp;
  }
}
