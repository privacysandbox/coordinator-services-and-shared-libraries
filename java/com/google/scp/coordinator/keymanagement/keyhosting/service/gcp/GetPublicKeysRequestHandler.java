package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.api.model.HttpMethod.GET;
import static com.google.scp.shared.api.util.RequestUtil.validateHttpMethod;
import static com.google.scp.shared.gcp.util.CloudFunctionUtil.createCloudFunctionResponseFromProto;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableMap;
import com.google.common.net.HttpHeaders;
import com.google.common.net.MediaType;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.GetActivePublicKeysResponseWithHeaders;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysRequestProto.GetActivePublicKeysRequest;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandlerBase;
import java.io.IOException;

/**
 * Handles requests to PublicKey Http Cloud Function and returns HTTP Response. Body is list of
 * public keys if successful or an ErrorResponse with error info.
 */
public class GetPublicKeysRequestHandler
    extends CloudFunctionRequestHandlerBase<
        GetActivePublicKeysRequest, GetActivePublicKeysResponseWithHeaders> {

  private final KeyService keyService;

  /** Creates a new instance of the {@code GetPublicKeysRequestHandler} class. */
  @Inject
  public GetPublicKeysRequestHandler(KeyService keyService) {
    this.keyService = keyService;
  }

  @Override
  protected GetActivePublicKeysRequest toRequest(HttpRequest httpRequest) throws ServiceException {
    validateHttpMethod(httpRequest.getMethod(), GET);
    return GetActivePublicKeysRequest.newBuilder().build();
  }

  @Override
  protected GetActivePublicKeysResponseWithHeaders processRequest(
      GetActivePublicKeysRequest getActivePublicKeysRequest) throws ServiceException {
    return keyService.getActivePublicKeys();
  }

  @Override
  protected void toCloudFunctionResponse(
      HttpResponse httpResponse, GetActivePublicKeysResponseWithHeaders response)
      throws IOException {
    if (response.cacheControlMaxAge().isPresent()) {
      createCloudFunctionResponseFromProto(
          httpResponse,
          response.getActivePublicKeysResponse(),
          OK.getHttpStatusCode(),
          /* headers= */ ImmutableMap.of(
              HttpHeaders.CONTENT_TYPE,
              MediaType.JSON_UTF_8.toString(),
              HttpHeaders.CACHE_CONTROL,
              response.cacheControlMaxAge().get()));
    } else {
      createCloudFunctionResponseFromProto(
          httpResponse,
          response.getActivePublicKeysResponse(),
          OK.getHttpStatusCode(),
          ImmutableMap.of(HttpHeaders.CONTENT_TYPE, MediaType.JSON_UTF_8.toString()));
    }
  }
}
