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

package com.google.scp.operator.frontend.service.converter;

import com.google.common.base.Converter;
import com.google.common.collect.ImmutableMap;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo;
import java.util.HashMap;
import java.util.Map;

/**
 * Converts between the {@link
 * com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest} and {@link
 * com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo}.
 */
public final class CreateJobRequestToRequestInfoConverter
    extends Converter<CreateJobRequest, RequestInfo> {

  /** Converts the frontend CreateJobRequest model into the shared RequestInfo model. */
  @Override
  protected RequestInfo doForward(CreateJobRequest createJobRequest) {
    return RequestInfo.newBuilder()
        .setJobRequestId(createJobRequest.getJobRequestId())
        .setInputDataBucketName(createJobRequest.getInputDataBucketName())
        .setInputDataBlobPrefix(createJobRequest.getInputDataBlobPrefix())
        .setOutputDataBucketName(createJobRequest.getOutputDataBucketName())
        .setOutputDataBlobPrefix(createJobRequest.getOutputDataBlobPrefix())
        .setPostbackUrl(createJobRequest.getPostbackUrl())
        .setAccountIdentity(createJobRequest.getAccountIdentity())
        .putAllJobParameters(createJobRequest.getJobParametersMap())
        .build();
  }

  /** Converts the shared RequestInfo model into the frontend CreateJobRequest model. */
  @Override
  protected CreateJobRequest doBackward(RequestInfo requestInfo) {
    Map<String, String> jobParameters = new HashMap<>(requestInfo.getJobParameters());
    return CreateJobRequest.newBuilder()
        .setJobRequestId(requestInfo.getJobRequestId())
        .setInputDataBucketName(requestInfo.getInputDataBucketName())
        .setInputDataBlobPrefix(requestInfo.getInputDataBlobPrefix())
        .setOutputDataBucketName(requestInfo.getOutputDataBucketName())
        .setOutputDataBlobPrefix(requestInfo.getOutputDataBlobPrefix())
        .setPostbackUrl(requestInfo.getPostbackUrl())
        .setAccountIdentity(requestInfo.getAccountIdentity())
        .putAllJobParameters(ImmutableMap.copyOf(jobParameters))
        .build();
  }
}
