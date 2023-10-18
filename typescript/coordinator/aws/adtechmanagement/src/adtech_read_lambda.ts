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

import { Adtech, HttpApiEvent, ReadRequest } from "../util/models";
import {
  converter,
  readAllFromAdtechMgmtDB,
  readFromAdtechMgmtDB,
} from "../util/db_operations";
import {
  INTERNAL_SERVER_ERROR_STATUS_CODE,
  READ_ALL_ADTECHS_ERROR_MESSAGE,
  REQUEST_SUCCESS_STATUS_CODE,
} from "../util/global_vars";
import { NativeAttributeValue } from "@aws-sdk/util-dynamodb";
/**
 * Handles the core logic of reading an Adtech.
 * @param {HttpApiEvent} event - The event object passed from the API Gateway to the Lambda containing the request body
 */
export const handler = async (event: HttpApiEvent) => {
  const readAdtechRequest: ReadRequest = JSON.parse(event.body); // contains adtech IAM role ARN in request body
  try {
    let adtechs: Adtech[];
    if (readAdtechRequest.iamRoleArn) {
      // if an IAM role ARN is passed, retrieve the corresponding adtech and convert from Record type to Adtech type
      adtechs = [
        converter(await readFromAdtechMgmtDB(readAdtechRequest.iamRoleArn)),
      ];
    } else {
      // if no IAM role ARN is passed, retrieve all adtechs and convert each one from Record type to Adtech type
      adtechs = (await readAllFromAdtechMgmtDB()).map(
        (recordAdtech: Record<string, NativeAttributeValue>) =>
          converter(recordAdtech),
      );
    }
    return {
      statusCode: REQUEST_SUCCESS_STATUS_CODE,
      body: JSON.stringify(adtechs),
    };
  } catch (error) {
    let message;
    if (readAdtechRequest.iamRoleArn) {
      message = `Adtech with iam role ARN ${readAdtechRequest.iamRoleArn} does not exist.`;
    } else {
      message = READ_ALL_ADTECHS_ERROR_MESSAGE;
    }
    return {
      statusCode: INTERNAL_SERVER_ERROR_STATUS_CODE,
      message: message,
    };
  }
};
