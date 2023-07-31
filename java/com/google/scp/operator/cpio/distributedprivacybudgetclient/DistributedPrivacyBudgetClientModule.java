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

package com.google.scp.operator.cpio.distributedprivacybudgetclient;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.inject.BindingAnnotation;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/**
 * Module class which binds the Transaction interfaces to the actual implementations. Also, one
 * {@link PrivacyBudgetClient} instance is generated for each coordinator. Each client sends
 * requests to the baseUrl for the specific coordinator. The authentication headers for each request
 * contain a signature generated against the provided auth endpoints. PBS Servers forward the
 * request to these auth endpoints for authentication
 */
public class DistributedPrivacyBudgetClientModule extends DistributedPrivacyBudgetClientBaseModule {

  @Override
  protected Class<? extends DistributedPrivacyBudgetClient>
      getDistributedPrivacyBudgetClientImplementation() {
    return DistributedPrivacyBudgetClientImpl.class;
  }

  @Override
  protected void configureModule() {
    bind(TransactionEngine.class).to(TransactionEngineImpl.class);
    bind(TransactionManager.class).to(TransactionManagerImpl.class);
    bind(TransactionPhaseManager.class).to(TransactionPhaseManagerImpl.class);
  }
  /**
   * Full URL (including protocol and api version path fragment) of coordinator A's privacy
   * budgeting service. Do not include trailing slash
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorAPrivacyBudgetServiceBaseUrl {}

  /**
   * Full URL (including protocol and api version path fragment) of coordinator B's privacy
   * budgeting service. Do not include trailing slash
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorBPrivacyBudgetServiceBaseUrl {}

  /** Auth endpoint of coordinator A's privacy budgeting service. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorAPrivacyBudgetServiceAuthEndpoint {}

  /** Auth endpoint of coordinator B's privacy budgeting service. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorBPrivacyBudgetServiceAuthEndpoint {}
}
