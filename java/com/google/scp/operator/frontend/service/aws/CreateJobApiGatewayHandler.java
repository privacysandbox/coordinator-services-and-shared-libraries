/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.operator.frontend.service.aws;

import static com.google.scp.shared.api.model.Code.ACCEPTED;
import static com.google.scp.shared.aws.util.LambdaHandlerUtil.createApiGatewayResponseFromProto;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.scp.operator.frontend.injection.factories.FrontendServicesFactory;
import com.google.scp.operator.frontend.service.FrontendService;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobResponseProto.CreateJobResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.aws.util.ApiGatewayHandler;
import java.util.ArrayList;
import java.util.List;

/** CreateJobApiGatewayHandler handles CreateJobRequests for the front end service rest api */
public final class CreateJobApiGatewayHandler
    extends ApiGatewayHandler<CreateJobRequest, CreateJobResponse> {

  private final FrontendService frontendService;

  /** Creates a new instance of the {@code CreateJobApiGatewayHandler} class. */
  public CreateJobApiGatewayHandler() {
    frontendService = FrontendServicesFactory.getFrontendService();
  }

  @Override
  protected CreateJobRequest toRequest(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context)
      throws ServiceException {
    String json = apiGatewayProxyRequestEvent.getBody();
    CreateJobRequest.Builder protoBuilder = CreateJobRequest.newBuilder();
    try {
      parser.merge(json, protoBuilder);
      CreateJobRequest request = protoBuilder.build();
      validateProperties(request, json);

      return request;
    } catch (InvalidProtocolBufferException e) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT, ErrorReasons.JSON_ERROR.toString(), e.getMessage());
    }
  }

  @Override
  protected CreateJobResponse processRequest(CreateJobRequest createJobRequest)
      throws ServiceException {
    return frontendService.createJob(createJobRequest);
  }

  @Override
  protected APIGatewayProxyResponseEvent toApiGatewayResponse(CreateJobResponse response) {
    // Return ACCEPTED code as the job has been queued and may not have started
    return createApiGatewayResponseFromProto(response, ACCEPTED.getHttpStatusCode(), allHeaders());
  }

  private void validateProperties(CreateJobRequest request, String json) throws ServiceException {
    List<String> missingProps = new ArrayList<>();
    if (request.getJobRequestId().isEmpty()) {
      missingProps.add("jobRequestId");
    }
    if (request.getInputDataBlobPrefix().isEmpty()) {
      missingProps.add("inputDataBlobPrefix");
    }
    if (request.getInputDataBucketName().isEmpty()) {
      missingProps.add("inputDataBucketName");
    }
    if (request.getOutputDataBlobPrefix().isEmpty()) {
      missingProps.add("outputDataBlobPrefix");
    }
    if (request.getOutputDataBucketName().isEmpty()) {
      missingProps.add("outputDataBucketName");
    }

    if (missingProps.size() > 0) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          ErrorReasons.JSON_ERROR.toString(),
          "Missing required properties: " + String.join(" ", missingProps) + "\r\n in: " + json);
    }
  }
}
