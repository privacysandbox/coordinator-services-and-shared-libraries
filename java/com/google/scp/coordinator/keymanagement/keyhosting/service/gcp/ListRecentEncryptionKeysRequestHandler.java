package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.api.model.HttpMethod.GET;
import static com.google.scp.shared.api.util.RequestUtil.validateHttpMethod;
import static com.google.scp.shared.gcp.util.CloudFunctionUtil.createCloudFunctionResponseFromProto;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysRequestProto.ListRecentEncryptionKeysRequest;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.exception.SharedErrorReason;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandlerBase;
import java.io.IOException;
import java.util.Optional;

/** Request handler for fetching a specific encrypted key. */
public class ListRecentEncryptionKeysRequestHandler
    extends CloudFunctionRequestHandlerBase<
        ListRecentEncryptionKeysRequest, ListRecentEncryptionKeysResponse> {

  private static final String MAX_AGE_SECONDS_PARAM_NAME =
      ListRecentEncryptionKeysRequest.getDescriptor()
          .findFieldByNumber(ListRecentEncryptionKeysRequest.MAX_AGE_SECONDS_FIELD_NUMBER)
          .getJsonName();
  private final KeyService keyService;

  /** Creates a new instance of the {@code ListRecentEncryptionKeysRequestHandler} class. */
  @Inject
  public ListRecentEncryptionKeysRequestHandler(KeyService keyService) {
    this.keyService = keyService;
  }

  @Override
  protected ListRecentEncryptionKeysRequest toRequest(HttpRequest httpRequest)
      throws ServiceException {
    validateHttpMethod(httpRequest.getMethod(), GET);

    return ListRecentEncryptionKeysRequest.newBuilder()
        .setMaxAgeSeconds(getMaxAgeSeconds(httpRequest))
        .build();
  }

  @Override
  protected ListRecentEncryptionKeysResponse processRequest(
      ListRecentEncryptionKeysRequest ListRecentEncryptionKeysRequest) throws ServiceException {
    return keyService.listRecentKeys(ListRecentEncryptionKeysRequest);
  }

  @Override
  protected void toCloudFunctionResponse(
      HttpResponse httpResponse, ListRecentEncryptionKeysResponse response) throws IOException {
    createCloudFunctionResponseFromProto(
        httpResponse, response, OK.getHttpStatusCode(), ImmutableMap.of());
  }

  private int getMaxAgeSeconds(HttpRequest httpRequest) throws ServiceException {
    try {
      Optional<String> maxAgeSeconds =
          httpRequest.getFirstQueryParameter(MAX_AGE_SECONDS_PARAM_NAME);
      if (maxAgeSeconds.isEmpty()) {
        throw new ServiceException(
            Code.INVALID_ARGUMENT,
            SharedErrorReason.INVALID_ARGUMENT.name(),
            String.format("%s query parameter is required.", MAX_AGE_SECONDS_PARAM_NAME));
      }
      return Integer.parseInt(maxAgeSeconds.get());
    } catch (NumberFormatException e) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.name(),
          String.format("%s should be a valid integer.", MAX_AGE_SECONDS_PARAM_NAME),
          e);
    }
  }
}
