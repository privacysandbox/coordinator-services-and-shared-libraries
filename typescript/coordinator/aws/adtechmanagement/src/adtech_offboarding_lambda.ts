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

"use strict";

import { Adtech, HttpApiEvent, OffboardingRequest } from "../util/models";
import {
  ADTECH_MGMT_CHECKPOINT,
  IAM_ROLE_CHECKPOINT,
  IAM_ROLE_POLICY_CHECKPOINT,
  INTERNAL_SERVER_ERROR_STATUS_CODE,
  REQUEST_SUCCESS_STATUS_CODE,
} from "../util/global_vars";
import {
  attachRolePolicy,
  createIAMRole,
  deleteIAMRole,
  detachRolePolicy,
} from "../util/role_operations";
import {
  addToAdtechMgmtDB,
  deleteFromAdtechMgmtDB,
  deleteFromPBSAuthDB,
} from "../util/db_operations";

/**
 * Rolls back each step of the offboarding process that succeeded before the failing step.
 *
 * @param {string} checkpoint - A checkpoint describing the operations that needs to be undone
 * @param {string} value - The value needed to undo the operation described by the checkpoint
 * @returns {void}
 */
const rollbackCheckpoint = async (
  checkpoint: string,
  value: string,
): Promise<void> => {
  switch (checkpoint) {
    case IAM_ROLE_POLICY_CHECKPOINT: {
      await attachRolePolicy(value);
      break;
    }
    case IAM_ROLE_CHECKPOINT: {
      await createIAMRole(value);
      break;
    }
    case ADTECH_MGMT_CHECKPOINT: {
      const adtech: Adtech = JSON.parse(value);
      await addToAdtechMgmtDB(adtech);
      break;
    }
  }
};
/**
 * Handles the core logic of offboarding an Adtech.
 * @param {HttpApiEvent} event - The event object passed from the API Gateway to the Lambda containing the request body
 */
export const handler = async (event: HttpApiEvent) => {
  // TODO: (b/291814116) remove all logging statements
  const adtechOffboardingRequest: OffboardingRequest = JSON.parse(event.body); // contains the adtech details from the request body
  console.log("offboarding request", adtechOffboardingRequest);
  const offboardingCheckpoints = new Map<string, string>();
  try {
    await detachRolePolicy(adtechOffboardingRequest.iamRoleName);
    console.log("Offboarding - detach policy from IAM role");
    offboardingCheckpoints.set(
      IAM_ROLE_POLICY_CHECKPOINT,
      adtechOffboardingRequest.iamRoleName,
    );

    await deleteIAMRole(adtechOffboardingRequest.iamRoleName);
    console.log("Offboarding - deleted IAM role");
    offboardingCheckpoints.set(
      IAM_ROLE_CHECKPOINT,
      adtechOffboardingRequest.awsID,
    );

    await deleteFromAdtechMgmtDB(adtechOffboardingRequest.iamRoleArn);
    console.log("Offboarding - deleted adtech metadata from adtech mgmt db");
    offboardingCheckpoints.set(
      ADTECH_MGMT_CHECKPOINT,
      JSON.stringify(adtechOffboardingRequest),
    );

    await deleteFromPBSAuthDB(adtechOffboardingRequest.iamRoleArn);
    console.log(`Offboarded ${adtechOffboardingRequest.adtechID} successfully`);

    return {
      statusCode: REQUEST_SUCCESS_STATUS_CODE,
      body: JSON.stringify({
        message: `Adtech ${adtechOffboardingRequest.adtechID} offboarded successfully`,
      }),
    };
  } catch (error) {
    console.log(error);
    for (const [checkpoint, value] of offboardingCheckpoints.entries()) {
      await rollbackCheckpoint(checkpoint, value);
    }

    return {
      statusCode: INTERNAL_SERVER_ERROR_STATUS_CODE,
      body: JSON.stringify({
        message: `Adtech ${adtechOffboardingRequest.adtechID} offboarding failed`,
      }),
    };
  }
};
