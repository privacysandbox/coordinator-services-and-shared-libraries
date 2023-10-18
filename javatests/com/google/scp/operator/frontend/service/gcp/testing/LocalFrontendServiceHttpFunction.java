package com.google.scp.operator.frontend.service.gcp.testing;

import com.google.inject.Guice;
import com.google.inject.Inject;
import com.google.inject.Injector;
import com.google.inject.Key;
import com.google.scp.operator.frontend.service.gcp.CreateJobRequestHandler;
import com.google.scp.operator.frontend.service.gcp.FrontendServiceHttpFunctionBase;
import com.google.scp.operator.frontend.service.gcp.GcpFrontendServiceModule.FrontendServiceVersionBinding;
import com.google.scp.operator.frontend.service.gcp.GetJobByIdRequestHandler;
import com.google.scp.operator.frontend.service.gcp.GetJobRequestHandler;
import com.google.scp.operator.frontend.service.gcp.PutJobRequestHandler;

/** Local http function for frontend service integration test */
public final class LocalFrontendServiceHttpFunction extends FrontendServiceHttpFunctionBase {
  public LocalFrontendServiceHttpFunction() {
    this(Guice.createInjector(new FrontendServiceTestModule()));
  }

  @Inject
  public LocalFrontendServiceHttpFunction(Injector injector) {
    super(
        injector.getInstance(CreateJobRequestHandler.class),
        injector.getInstance(GetJobRequestHandler.class),
        injector.getInstance(PutJobRequestHandler.class),
        injector.getInstance(GetJobByIdRequestHandler.class),
        injector.getInstance(Key.get(Integer.class, FrontendServiceVersionBinding.class)));
  }
}
