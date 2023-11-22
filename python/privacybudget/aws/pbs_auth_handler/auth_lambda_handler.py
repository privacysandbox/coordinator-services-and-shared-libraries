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
from publicsuffixlist import PublicSuffixList

"""Function to determine whether an entity is authorized to send requests to PBS.

This function checks the request's identity payload and parses out the user ARN value.
It checks the internal Dynamo DB table for the existence of said user ARN.
If the value exists, then authorization succeeds. Otherwise it does not.
"""

authorization_failure_stage = ""
psl = PublicSuffixList()

def lambda_handler(event, context):
    if (
        'headers' in event
        and 'x-gscp-enable-per-site-enrollment' in event['headers']
        and event['headers']['x-gscp-enable-per-site-enrollment'] == 'true'
    ):
        return handle_request_v2(event, context)
    else:
        return handle_request_v1(event, context)


# handle_request_v1 and _v2 contain identical code blocks to create additional
# layer of separation between each version. v2 is currently under trial, and
# this allows easier understanding of each version independent of the other.
def handle_request_v1(event, context):
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
        print('reported origin verification failed: ' + str(stage))
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}

    return {'statusCode': 200, 'body': json.dumps({'authorized_domain': reporting_origin})}

def handle_request_v2(event, context):
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

    stage = 'validateReportedOrigin'
    derived_site = convert_reported_origin_url_to_site(reported_origin)
    if not derived_site:
        authorization_failure_stage = stage
        print('authorization failure at stage: ' + str(stage))
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}
    print("Derived site from authorization request: ", derived_site)

    userArn = identity['userArn']
    if userArn[:11] == 'arn:aws:sts':
        roles = userArn.split('/')
        domain = roles[0].split(':')
        account = domain[4]
        userArn = 'arn:aws:iam::' + account + ':role/' + roles[1]
    try:
        dynamo = boto3.resource('dynamodb')
        table_name = os.environ['PBS_AUTHORIZATION_V2_DYNAMODB_TABLE_NAME']
        table = dynamo.Table(table_name)
        resp = table.get_item(Key={"adtech_identity": userArn})
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
    registered_adtech_sites = item['adtech_sites']
    print("Adtech sites allowlisted for the provided identity are: ", registered_adtech_sites)

    stage = 'authorizationDecision'
    if derived_site not in registered_adtech_sites:
        authorization_failure_stage = stage
        print('reported origin verification failed: ' + str(stage))
        return {'statusCode': 403, 'body': json.dumps('authorization forbidden')}

    return {'statusCode': 200, 'body': json.dumps({'authorized_domain': derived_site})}


def fix_protocol_prefix(adtech_site, reported_origin):
    if adtech_site == reported_origin:
        return adtech_site.replace("http://", "https://")
    return "https://" + adtech_site

def convert_reported_origin_url_to_site(claimed_identity_url):
    # The PSL library has inconsistent behaviour in retaining the protocol value from the input.
    # Hence we remove any http or https protocols from the input.
    # We force https to be the only allowed protocol. http is not allowed.
    # Thus we add https to the dervied site
    global psl
    claimed_identity_url = claimed_identity_url.replace('http://', '').replace(
        'https://', ''
    )
    private_suffix = psl.privatesuffix(claimed_identity_url)
    return 'https://' + private_suffix