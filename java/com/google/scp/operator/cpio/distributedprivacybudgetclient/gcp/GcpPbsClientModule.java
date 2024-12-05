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
package com.google.scp.operator.cpio.distributedprivacybudgetclient.gcp;

import com.google.common.collect.ImmutableList;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.configclient.Annotations.TrustedServicesClientVersion;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClientModule;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClient;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClientImpl;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClientImplV2;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClientV2;
import com.google.scp.shared.api.util.HttpClientWithInterceptor;
import com.google.scp.shared.gcp.util.GcpHttpInterceptorUtil;
import java.io.IOException;
import org.apache.hc.core5.http.HttpRequestInterceptor;

public class GcpPbsClientModule extends DistributedPrivacyBudgetClientModule {

  @Provides
  @Singleton
  public ImmutableList<PrivacyBudgetClient> privacyBudgetClients(
      @CoordinatorAPrivacyBudgetServiceBaseUrl String coordinatorAPrivacyBudgetServiceBaseUrl,
      @CoordinatorBPrivacyBudgetServiceBaseUrl String coordinatorBPrivacyBudgetServiceBaseUrl,
      @CoordinatorAPrivacyBudgetServiceAuthEndpoint
          String coordinatorAPrivacyBudgetServiceAuthEndpoint,
      @CoordinatorBPrivacyBudgetServiceAuthEndpoint
          String coordinatorBPrivacyBudgetServiceAuthEndpoint,
      @TrustedServicesClientVersion String trustedServicesClientVersion)
      throws IOException {
    HttpRequestInterceptor coordinatorATokenInterceptor =
        GcpHttpInterceptorUtil.createPbsHttpInterceptor(
            coordinatorAPrivacyBudgetServiceAuthEndpoint);
    HttpRequestInterceptor coordinatorBTokenInterceptor =
        GcpHttpInterceptorUtil.createPbsHttpInterceptor(
            coordinatorBPrivacyBudgetServiceAuthEndpoint);

    HttpClientWithInterceptor coordinatorAGcpHttpClient =
        new HttpClientWithInterceptor(coordinatorATokenInterceptor, trustedServicesClientVersion);

    HttpClientWithInterceptor coordinatorBGcpHttpClient =
        new HttpClientWithInterceptor(coordinatorBTokenInterceptor, trustedServicesClientVersion);

    return ImmutableList.of(
        new PrivacyBudgetClientImpl(
            coordinatorAGcpHttpClient, coordinatorAPrivacyBudgetServiceBaseUrl),
        new PrivacyBudgetClientImpl(
            coordinatorBGcpHttpClient, coordinatorBPrivacyBudgetServiceBaseUrl));
  }

  @Provides
  @Singleton
  public ImmutableList<PrivacyBudgetClientV2> privacyBudgetClientsV2(
      @CoordinatorAPrivacyBudgetServiceBaseUrl String coordinatorAPrivacyBudgetServiceBaseUrl,
      @CoordinatorBPrivacyBudgetServiceBaseUrl String coordinatorBPrivacyBudgetServiceBaseUrl,
      @CoordinatorAPrivacyBudgetServiceAuthEndpoint
          String coordinatorAPrivacyBudgetServiceAuthEndpoint,
      @CoordinatorBPrivacyBudgetServiceAuthEndpoint
          String coordinatorBPrivacyBudgetServiceAuthEndpoint,
      @TrustedServicesClientVersion String trustedServicesClientVersion)
      throws IOException {
    HttpRequestInterceptor coordinatorATokenInterceptor =
        GcpHttpInterceptorUtil.createPbsHttpInterceptor(
            coordinatorAPrivacyBudgetServiceAuthEndpoint);
    HttpRequestInterceptor coordinatorBTokenInterceptor =
        GcpHttpInterceptorUtil.createPbsHttpInterceptor(
            coordinatorBPrivacyBudgetServiceAuthEndpoint);

    HttpClientWithInterceptor coordinatorAGcpHttpClient =
        new HttpClientWithInterceptor(coordinatorATokenInterceptor, trustedServicesClientVersion);

    HttpClientWithInterceptor coordinatorBGcpHttpClient =
        new HttpClientWithInterceptor(coordinatorBTokenInterceptor, trustedServicesClientVersion);

    return ImmutableList.of(
        new PrivacyBudgetClientImplV2(
            coordinatorAGcpHttpClient, coordinatorAPrivacyBudgetServiceBaseUrl),
        new PrivacyBudgetClientImplV2(
            coordinatorBGcpHttpClient, coordinatorBPrivacyBudgetServiceBaseUrl));
  }
}
