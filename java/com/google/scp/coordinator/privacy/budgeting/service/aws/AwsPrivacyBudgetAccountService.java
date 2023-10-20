/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.privacy.budgeting.service.aws;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.google.common.base.Strings;
import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.dao.aws.AwsIdentityDb;
import com.google.scp.coordinator.privacy.budgeting.dao.aws.AwsIdentityDb.AwsIdentityDbException;
import com.google.scp.coordinator.privacy.budgeting.model.AdTechIdentity;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.exception.SharedErrorReason;
import com.google.scp.shared.api.model.Code;
import java.util.Objects;
import java.util.Optional;

/**
 * Handles request AuthN and AuthZ for the AWS privacy budget service
 *
 * <p>TODO(b/233384982) Split out AWS specific logic into handler and move business logic into a new
 * task class
 */
public final class AwsPrivacyBudgetAccountService {

  private final AwsIdentityDb awsIdentityDb;

  // TODO(b/233384982): Only access AwsIdentityDb from tasks layer
  @Inject
  AwsPrivacyBudgetAccountService(AwsIdentityDb awsIdentityDb) {
    this.awsIdentityDb = awsIdentityDb;
  }

  public void authorizeRequest(
      ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest,
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent)
      throws ServiceException {

    // Authentication: get the user ARN of the request
    String requestUserArn =
        apiGatewayProxyRequestEvent.getRequestContext().getIdentity().getUserArn();

    if (Strings.isNullOrEmpty(requestUserArn)) {
      throw new ServiceException(
          Code.PERMISSION_DENIED,
          SharedErrorReason.NOT_AUTHORIZED.toString(),
          "Unable to determine identity of requester. Request user ARN was null or empty");
    }

    // Authentication: get the IAM role from the user ARN
    String requesterIamRole = AwsIamRoles.extractIamRoleFromAssumedRoleUserArn(requestUserArn);

    // Authentication: get the attribution-report-to associated with this requestor
    Optional<AdTechIdentity> adTechIdentity;
    try {
      adTechIdentity = awsIdentityDb.getIdentity(requesterIamRole);
    } catch (AwsIdentityDbException e) {
      throw new ServiceException(
          Code.INTERNAL,
          SharedErrorReason.SERVER_ERROR.toString(),
          "Error reading from identity DB. Unable to authenticate request",
          e);
    }

    if (adTechIdentity.isEmpty()) {
      throw new ServiceException(
          Code.PERMISSION_DENIED,
          SharedErrorReason.NOT_AUTHORIZED.toString(),
          String.format(
              "Unable to determine identity of requester. No identity present in identity DB for"
                  + " requestUserArn=%s requesterIamRole=%s",
              requestUserArn, requesterIamRole));
    }

    // Authorization: check that the requester is consuming budget for the attribution-report-to
    // associated with their identity. This is done using a simple string match.
    if (!Objects.equals(
        adTechIdentity.get().attributionReportTo(),
        consumePrivacyBudgetRequest.attributionReportTo())) {
      throw new ServiceException(
          Code.PERMISSION_DENIED,
          SharedErrorReason.NOT_AUTHORIZED.toString(),
          String.format(
              "Requester with requestUserArn=%s and requesterIamRole=%s is not authorized to"
                  + " perform consume budget operations for attributionReportTo=%s. Allowed"
                  + " attributionReportTo value is:%s",
              requestUserArn,
              requesterIamRole,
              consumePrivacyBudgetRequest.attributionReportTo(),
              adTechIdentity.get().attributionReportTo()));
    }
  }
}
