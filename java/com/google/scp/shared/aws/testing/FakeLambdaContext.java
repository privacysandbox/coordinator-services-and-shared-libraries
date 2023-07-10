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

package com.google.scp.shared.aws.testing;

import com.amazonaws.services.lambda.runtime.ClientContext;
import com.amazonaws.services.lambda.runtime.CognitoIdentity;
import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.LambdaLogger;

/** Test fake for the {@link Context} used in lambda handlers */
public final class FakeLambdaContext implements Context {
  @Override
  public String getAwsRequestId() {
    return "";
  }

  @Override
  public String getLogGroupName() {
    return "";
  }

  @Override
  public String getLogStreamName() {
    return "";
  }

  @Override
  public String getFunctionName() {
    return "";
  }

  @Override
  public String getFunctionVersion() {
    return "";
  }

  @Override
  public String getInvokedFunctionArn() {
    return "";
  }

  @Override
  public CognitoIdentity getIdentity() {
    return null;
  }

  @Override
  public ClientContext getClientContext() {
    return null;
  }

  @Override
  public int getRemainingTimeInMillis() {
    return 0;
  }

  @Override
  public int getMemoryLimitInMB() {
    return 0;
  }

  @Override
  public LambdaLogger getLogger() {
    return null;
  }
}
