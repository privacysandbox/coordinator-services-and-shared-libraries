package com.google.scp.operator.frontend.service.gcp;

import static com.google.scp.shared.api.model.HttpMethod.GET;
import static com.google.scp.shared.api.model.HttpMethod.POST;

import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.api.model.HttpMethod;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandler;
import com.google.scp.shared.gcp.util.CloudFunctionServiceBase;
import java.util.regex.Pattern;

/** Base class for frontend service http function */
public class FrontendServiceHttpFunctionBase extends CloudFunctionServiceBase {
  public static final int JOB_V1 = 1;
  public static final int JOB_V2 = 2;
  private static final Pattern createJobUrlPattern =
      Pattern.compile("/v1alpha/createJob", Pattern.CASE_INSENSITIVE);
  private static final Pattern getJobUrlPattern =
      Pattern.compile("/v1alpha/getJob", Pattern.CASE_INSENSITIVE);

  protected final CreateJobRequestHandler createJobRequestHandler;
  protected final GetJobRequestHandler getJobRequestHandler;
  protected final PutJobRequestHandler putJobRequestHandler;
  protected final GetJobByIdRequestHandler getJobByIdRequestHandler;
  protected final int version;

  /**
   * Creates a new instance of the {@code FrontendServiceHttpFunction} class with the given {@link
   * CreateJobRequestHandler}, {@link GetJobRequestHandler}, {@link PutJobRequestHandler}, {@link
   * GetJobByIdRequestHandler} and version.
   */
  public FrontendServiceHttpFunctionBase(
      CreateJobRequestHandler createJobRequestHandler,
      GetJobRequestHandler getJobRequestHandler,
      PutJobRequestHandler putJobRequestHandler,
      GetJobByIdRequestHandler getJobByIdRequestHandler,
      int version) {
    this.createJobRequestHandler = createJobRequestHandler;
    this.getJobRequestHandler = getJobRequestHandler;
    this.putJobRequestHandler = putJobRequestHandler;
    this.getJobByIdRequestHandler = getJobByIdRequestHandler;
    this.version = version;
  }

  /**
   * {@link CreateJobRequestHandler} and {@link GetJobRequestHandler are for JOB_V1 and
   * {@link PutJobRequestHandler}, {@link GetJobByIdRequestHandler} are for JOB_V2.
   */
  @Override
  protected ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
      getRequestHandlerMap() {
    if (this.version == JOB_V1) {
      return ImmutableMap.of(
          POST,
          ImmutableMap.of(createJobUrlPattern, this.createJobRequestHandler),
          GET,
          ImmutableMap.of(getJobUrlPattern, this.getJobRequestHandler));
    }
    return ImmutableMap.of(
        POST,
        ImmutableMap.of(createJobUrlPattern, this.putJobRequestHandler),
        GET,
        ImmutableMap.of(getJobUrlPattern, this.getJobByIdRequestHandler));
  }
}
