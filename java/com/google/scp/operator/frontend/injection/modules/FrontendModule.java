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

package com.google.scp.operator.frontend.injection.modules;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.auto.service.AutoService;
import com.google.common.base.Converter;
import com.google.scp.operator.frontend.serialization.JsonSerializerFacade;
import com.google.scp.operator.frontend.serialization.ObjectMapperSerializerFacade;
import com.google.scp.operator.frontend.service.FrontendService;
import com.google.scp.operator.frontend.service.FrontendServiceImpl;
import com.google.scp.operator.frontend.service.converter.CreateJobRequestToRequestInfoConverter;
import com.google.scp.operator.frontend.service.converter.ErrorCountConverter;
import com.google.scp.operator.frontend.service.converter.ErrorSummaryConverter;
import com.google.scp.operator.frontend.service.converter.GetJobResponseConverter;
import com.google.scp.operator.frontend.service.converter.JobStatusConverter;
import com.google.scp.operator.frontend.service.converter.ResultInfoConverter;
import com.google.scp.operator.frontend.tasks.CreateJobTask;
import com.google.scp.operator.frontend.tasks.aws.AwsCreateJobTask;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.frontend.api.v1.ErrorCountProto;
import com.google.scp.operator.protos.frontend.api.v1.ErrorSummaryProto;
import com.google.scp.operator.protos.frontend.api.v1.GetJobResponseProto;
import com.google.scp.operator.protos.frontend.api.v1.JobStatusProto;
import com.google.scp.operator.protos.frontend.api.v1.ResultInfoProto;
import com.google.scp.operator.protos.shared.backend.ErrorCountProto.ErrorCount;
import com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary;
import com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo;
import com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.shared.mapper.TimeObjectMapper;

/** Configures all the modules required to run the frontend service. */
@AutoService(BaseFrontendModule.class)
public final class FrontendModule extends BaseFrontendModule {

  @Override
  public Class<? extends JsonSerializerFacade> getJsonSerializerImplementation() {
    return ObjectMapperSerializerFacade.class;
  }

  @Override
  public Class<? extends FrontendService> getFrontendServiceImplementation() {
    return FrontendServiceImpl.class;
  }

  @Override
  public Class<? extends Converter<JobMetadata, GetJobResponseProto.GetJobResponse>>
      getJobResponseConverterImplementation() {
    return GetJobResponseConverter.class;
  }

  @Override
  public Class<? extends Converter<CreateJobRequest, RequestInfo>>
      getCreateJobRequestToRequestInfoConverterImplementation() {
    return CreateJobRequestToRequestInfoConverter.class;
  }

  @Override
  public Class<? extends Converter<ResultInfo, ResultInfoProto.ResultInfo>>
      getResultInfoConverterImplementation() {
    return ResultInfoConverter.class;
  }

  @Override
  public Class<? extends Converter<ErrorSummary, ErrorSummaryProto.ErrorSummary>>
      getErrorSummaryConverterImplementation() {
    return ErrorSummaryConverter.class;
  }

  @Override
  public Class<
          ? extends
              Converter<
                  com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus,
                  JobStatusProto.JobStatus>>
      getJobStatusConverterImplementation() {
    return JobStatusConverter.class;
  }

  @Override
  public Class<? extends Converter<ErrorCount, ErrorCountProto.ErrorCount>>
      getErrorCountConverterImplementation() {
    return ErrorCountConverter.class;
  }

  @Override
  protected void configureModule() {
    bind(ObjectMapper.class).to(TimeObjectMapper.class);
    bind(CreateJobTask.class).to(AwsCreateJobTask.class);
  }
}
