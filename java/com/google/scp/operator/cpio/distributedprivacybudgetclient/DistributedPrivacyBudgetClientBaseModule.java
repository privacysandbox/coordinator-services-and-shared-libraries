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

import com.google.inject.AbstractModule;
import com.google.inject.Singleton;

/** Base for modules that expose DistributedPrivacyBudgetClients */
public abstract class DistributedPrivacyBudgetClientBaseModule extends AbstractModule {

  @Override
  protected void configure() {
    bind(DistributedPrivacyBudgetClient.class)
        .to(getDistributedPrivacyBudgetClientImplementation())
        .in(Singleton.class);
    configureModule();
  }

  /** Gets the implementation for the DistributedPrivacyBudgetClient */
  protected abstract Class<? extends DistributedPrivacyBudgetClient>
      getDistributedPrivacyBudgetClientImplementation();

  /**
   * Arbitrary configurations that can be done by the implementing class to support dependencies
   * that are specific to that implementation.
   */
  protected void configureModule() {}
}
