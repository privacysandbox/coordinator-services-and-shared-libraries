//  Copyright 2023 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

import {
  addToAdtechMgmtDB,
  addToPBSAuthDB,
  deleteFromAdtechMgmtDB,
} from "../util/db_operations";
import {
  attachRolePolicy,
  createIAMRole,
  deleteIAMRole,
  detachRolePolicy,
} from "../util/role_operations";
import {
  ADTECH_MGMT_CHECKPOINT,
  PBS_AUTH_CHECKPOINT,
  IAM_ROLE_CHECKPOINT,
  IAM_ROLE_POLICY_CHECKPOINT,
  INTERNAL_SERVER_ERROR_STATUS_CODE,
  REQUEST_SUCCESS_STATUS_CODE,
  MALFORMED_POLICY_CUSTOM_ERROR_MESSAGE,
} from "../util/global_vars";
import { Adtech, HttpApiEvent, OnboardingRequest } from "../util/models";
import { Role } from "@aws-sdk/client-iam";
/**
 * Rolls back each step of the offboarding process that succeeded before the failing step
 * @param {string} checkpoint - A checkpoint describing the operations that needs to be undone
 * @param {string} value - The value needed to undo the operation described by the checkpoint
 * @returns {void}
 */
const rollbackCheckpoint = async (
  checkpoint: string,
  value: string,
): Promise<void> => {
  switch (checkpoint) {
    case IAM_ROLE_CHECKPOINT: {
      await deleteIAMRole(value);
      break;
    }
    case IAM_ROLE_POLICY_CHECKPOINT: {
      await detachRolePolicy(value);
      break;
    }
    case ADTECH_MGMT_CHECKPOINT: {
      await deleteFromAdtechMgmtDB(value);
      break;
    }
  }
};
/**
 * Handles the core logic of onboard an Adtech.
 * @param {HttpApiEvent} event - The event object passed from the API Gateway to the Lambda containing the request body
 */
export const handler = async (event: HttpApiEvent) => {
  // TODO: (b/291814116) remove all logging statements
  const adtechOnboardingRequest: OnboardingRequest = JSON.parse(event.body); // contains the adtech details from the request body
  const onboardingCheckpoints = new Map<string, string>();
  try {
    const iamRole: Role = await createIAMRole(adtechOnboardingRequest.awsID);
    console.log("Onboarding - created IAM role");
    onboardingCheckpoints.set(IAM_ROLE_CHECKPOINT, iamRole.RoleName!);

    await attachRolePolicy(iamRole.RoleName!);
    console.log("Onboarding - attached role policy");
    onboardingCheckpoints.set(IAM_ROLE_POLICY_CHECKPOINT, iamRole.RoleName!);

    const adtechMetadata: Adtech = {
      awsID: adtechOnboardingRequest.awsID,
      adtechID: adtechOnboardingRequest.adtechID,
      originURL: adtechOnboardingRequest.originURL,
      iamRoleArn: iamRole.Arn!,
      iamRoleName: iamRole.RoleName!,
    };

    await addToAdtechMgmtDB(adtechMetadata);
    console.log("Onboarding - added to adtech mgmt db");
    onboardingCheckpoints.set(
      ADTECH_MGMT_CHECKPOINT,
      adtechMetadata.iamRoleArn,
    );

    await addToPBSAuthDB(adtechMetadata.iamRoleArn, adtechMetadata.originURL);
    console.log(`onboarded ${adtechOnboardingRequest.adtechID} successfully`);

    return {
      statusCode: REQUEST_SUCCESS_STATUS_CODE,
      body: JSON.stringify({
        message: `Adtech ${adtechOnboardingRequest.adtechID} onboarded successfully`,
      }),
    };
  } catch (error: any) {
    for (const [checkpoint, value] of onboardingCheckpoints.entries()) {
      await rollbackCheckpoint(checkpoint, value);
    }
    let message: string = "";
    if (error instanceof Error) {
      const errorType = error.name;
      switch (errorType) {
        case "MalformedPolicyDocumentException":
          message = MALFORMED_POLICY_CUSTOM_ERROR_MESSAGE;
          break;
        case "EntityAlreadyExistsException":
          message = `An Adtech with AWS ID ${adtechOnboardingRequest.awsID} is already onboarded.`;
          break;
        default:
          message = error.message;
          break;
      }
      return {
        statusCode: INTERNAL_SERVER_ERROR_STATUS_CODE,
        body: JSON.stringify({
          message: message,
        }),
      };
    }
    return {
      statusCode: INTERNAL_SERVER_ERROR_STATUS_CODE,
      body: JSON.stringify({
        message: error,
      }),
    };
  }
};
