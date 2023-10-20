package com.google.scp.shared.clients.configclient.gcp;

import com.google.api.gax.rpc.NotFoundException;
import com.google.cloud.secretmanager.v1.AccessSecretVersionResponse;
import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.google.common.cache.CacheBuilder;
import com.google.common.cache.CacheLoader;
import com.google.common.cache.LoadingCache;
import com.google.inject.Inject;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClientUtils;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpClientConfigMetadataServiceClient;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpProjectId;
import com.google.scp.shared.clients.configclient.model.ErrorReason;
import java.io.IOException;
import java.util.Optional;
import java.util.concurrent.TimeUnit;
import javax.inject.Singleton;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Parameter client implementation for getting runtime parameters from GCP secret manager */
@Singleton
public final class GcpParameterClient implements ParameterClient {

  private static final Logger logger = LoggerFactory.getLogger(GcpParameterClient.class);
  private static final int MAX_CACHE_SIZE = 100;
  private static final long CACHE_ENTRY_TTL_SEC = 3600;
  private static final String DEFAULT_PARAM_PREFIX = "scp";
  private static final String METADATA_ENVIRONMENT_KEY = "scp-environment";

  private final Supplier<String> environmentSupplier =
      Suppliers.memoizeWithExpiration(this::loadEnvironment, CACHE_ENTRY_TTL_SEC, TimeUnit.SECONDS);
  private final LoadingCache<String, Optional<String>> paramCache =
      CacheBuilder.newBuilder()
          .maximumSize(MAX_CACHE_SIZE)
          .expireAfterWrite(CACHE_ENTRY_TTL_SEC, TimeUnit.SECONDS)
          .build(
              new CacheLoader<>() {
                // Wrap as unchecked since Suppliers::memoize doesn't allow checked exceptions
                @Override
                public Optional<String> load(String key) throws ParameterClientException {
                  return getParameterValue(key);
                }
              });

  private final SecretManagerServiceClientProxy secretManagerServiceClient;
  private final GcpMetadataServiceClient metadataServiceClient;
  private final String projectId;

  @Inject
  public GcpParameterClient(
      SecretManagerServiceClientProxy secretManagerServiceClient,
      @GcpClientConfigMetadataServiceClient GcpMetadataServiceClient metadataServiceClient,
      @GcpProjectId String projectId) {
    this.secretManagerServiceClient = secretManagerServiceClient;
    this.metadataServiceClient = metadataServiceClient;
    this.projectId = projectId;
  }

  @Override
  public Optional<String> getParameter(String param) throws ParameterClientException {
    return getParameter(
        param, Optional.of(DEFAULT_PARAM_PREFIX), /* includeEnvironmentParam= */ true);
  }

  @Override
  public Optional<String> getParameter(
      String param, Optional<String> paramPrefix, boolean includeEnvironmentParam)
      throws ParameterClientException {
    String storageParam =
        ParameterClientUtils.getStorageParameterName(
            param, paramPrefix, includeEnvironmentParam ? getEnvironmentName() : Optional.empty());
    try {
      return paramCache.get(storageParam);
    } catch (Exception ex) {
      throw new ParameterClientException(
          String.format("Error reading parameter %s from GCP param cache.", storageParam),
          ErrorReason.FETCH_ERROR,
          ex);
    }
  }

  @Override
  public Optional<String> getEnvironmentName() {
    return Optional.of(environmentSupplier.get());
  }

  private Optional<String> getParameterValue(String param) throws ParameterClientException {
    String secretName = String.format("projects/%s/secrets/%s/versions/latest", projectId, param);

    logger.info("Fetching parameter %s from GCP secret manager.".format(secretName));
    AccessSecretVersionResponse response;
    try {
      response = this.secretManagerServiceClient.accessSecretVersion(secretName);
    } catch (NotFoundException ex) {
      return Optional.empty();
    } catch (Exception ex) {
      throw new ParameterClientException(
          String.format("Error reading parameter %s from GCP secret manager.", param),
          ErrorReason.FETCH_ERROR,
          ex);
    }

    return Optional.of(response.getPayload().getData().toStringUtf8());
  }

  private String loadEnvironment() {
    try {
      return metadataServiceClient
          .getMetadata(METADATA_ENVIRONMENT_KEY)
          .orElseThrow(
              () ->
                  new IllegalStateException(
                      String.format(
                          "Environment missing, metadata field '%s' not found on instance.",
                          METADATA_ENVIRONMENT_KEY)));

    } catch (IOException e) {
      throw new IllegalStateException("Failed to fetch environment from instance metadata.", e);
    }
  }
}
