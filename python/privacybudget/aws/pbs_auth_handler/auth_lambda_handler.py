# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import json
import boto3

"""Function to determine whether an entity is authorized to send requests to PBS.

This function checks the request's identity payload and parses out the user ARN value.
It checks the internal Dynamo DB table for the existence of said user ARN.
If the value exists, then authorization succeeds. Otherwise it does not.
"""

authorization_failure_stage = ""


def lambda_handler(event, context):
    global authorization_failure_stage

    stage = 'context'
    if 'requestContext' not in event:
        authorization_failure_stage = stage
        print('authorization failed at stage: ' + str(stage))
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}

    stage = 'identity'
    requestContext = event['requestContext']
    if 'identity' not in requestContext:
        authorization_failure_stage = stage
        print('authorization failed at stage: ' + str(stage))
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}

    identity = requestContext['identity']
    stage = 'userArn'
    if 'userArn' not in identity:
        authorization_failure_stage = stage
        print('authorization failed at stage: ' + str(stage))
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}

    stage = 'reportedOrigin'
    reported_origin = ''
    if 'headers' in event and 'x-gscp-claimed-identity' in event['headers']:
        reported_origin = event['headers']['x-gscp-claimed-identity']
    if not reported_origin:
        authorization_failure_stage = stage
        print('authorization failed at stage: ' + str(stage))
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}

    userArn = identity['userArn']
    if userArn[:11] == 'arn:aws:sts':
        roles = userArn.split('/')
        domain = roles[0].split(':')
        account = domain[4]
        userArn = 'arn:aws:iam::' + account + ':role/' + roles[1]
    try:
        dynamo = boto3.resource('dynamodb')
        table_name = os.environ['REPORTING_ORIGIN_AUTH_DYNAMO_DB_TABLE_NAME']
        table = dynamo.Table(table_name)
        resp = table.get_item(Key={"iam_role": userArn})
    except Exception as ex:
        authorization_failure_stage = stage
        print(
            'authorization forbidden, cannot query with key: [' + userArn + '] exception:' + str(ex))
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}

    if 'Item' not in resp:
        authorization_failure_stage = stage
        print('authorization forbidden, no result with key: [' + userArn + ']')
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}

    item = resp['Item']
    reporting_origin = item['reporting_origin']

    stage = 'reportingOrigin'
    if reporting_origin != reported_origin:
        authorization_failure_stage = stage
        print('reported origin verficiation failed: ' + str(stage))
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}

    return {'statusCode': 200, 'body': json.dumps({'authorized_domain': reporting_origin})}
