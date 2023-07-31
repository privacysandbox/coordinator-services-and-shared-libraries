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

import com.google.inject.Module;
import com.google.scp.operator.frontend.injection.modules.testing.FakeFrontendModule;
import com.google.scp.operator.frontend.service.ServiceJobGenerator;
import com.google.scp.operator.protos.frontend.api.v1.ErrorSummaryProto;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.shared.testutils.common.ConverterTest;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class ErrorSummaryConverterTest
    extends ConverterTest<
        com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary,
        ErrorSummaryProto.ErrorSummary> {

  @Override
  public com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary getFrom() {
    return JobGenerator.createFakeErrorSummaryShared();
  }

  @Override
  public ErrorSummaryProto.ErrorSummary getTo() {
    return ServiceJobGenerator.CreateFakeErrorSummary();
  }

  @Override
  public Class<? extends Module> getModule() {
    return FakeFrontendModule.class;
  }
}
