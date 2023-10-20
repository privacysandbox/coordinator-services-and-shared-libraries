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

import base64
import functions_framework
import json
import os
from google.cloud import spanner

FAILURE = -1

add_failure_stage_context = False


def forbidden(stage):
    if (add_failure_stage_context):
        return (json.dumps('authorization forbidden'), 403, stage)
    else:
        return (json.dumps('authorization forbidden'), 403)


def get_reported_origin(headers):
    if ('x-gscp-claimed-identity' not in headers):
        return FAILURE

    return headers['x-gscp-claimed-identity']


def get_caller_identity(headers):
    try:
        id_token = headers['Authorization']
        token_claims = id_token.split('.')[1]
        # A base64 string's length must always be a multiple of 4, and
        # the Python decoder is very strict about this length check.
        # So we add extra padding to guarantee that the base64 decode
        # won't fail due to the length of the encoded string chunk.
        token_claims = base64.b64decode(
            token_claims + ('=' * (-len(token_claims) % 4)))
        token_claims = json.loads(token_claims)

        if not 'email' in token_claims:
            return FAILURE

        return token_claims['email']
    except Exception as e:
        print(e)
        return FAILURE


def get_reporting_origin(caller_identity):
    try:
        instance_id = os.environ['REPORTING_ORIGIN_AUTH_SPANNER_INSTANCE_ID']
        database_id = os.environ['REPORTING_ORIGIN_AUTH_SPANNER_DATABASE_ID']
        table_name = os.environ['REPORTING_ORIGIN_AUTH_SPANNER_TABLE_NAME']

        client = spanner.Client()

        instance = client.instance(instance_id)
        database = instance.database(database_id)

        reporting_origin = ''

        with database.snapshot() as snapshot:
            query = "SELECT ro.ReportingOriginUrl FROM {} ro WHERE AccountId = @accountId".format(
                table_name)
            results = snapshot.execute_sql(
                query,
                params={'accountId': caller_identity},
                param_types={'accountId': spanner.param_types.STRING})
            # Get the expected one result or throw
            reporting_origin = results.one()[0]

        if not reporting_origin:
            return FAILURE

        return reporting_origin
    except Exception as e:
        print(e)
        return FAILURE


@functions_framework.http
def function_handler(request):
    """HTTP Cloud Function.
    Args:
        request (flask.Request): The request object.
        <https://flask.palletsprojects.com/en/1.1.x/api/#incoming-request-data>
    Returns:
        The response text, or any set of values that can be turned into a
        Response object using `make_response`
        <https://flask.palletsprojects.com/en/1.1.x/api/#flask.make_response>.
    """
    headers = request.headers

    reported_origin = get_reported_origin(headers)
    if reported_origin is FAILURE:
        print('Failed to get reported origin from headers')
        return forbidden('reported_origin')

    caller_identity = get_caller_identity(headers)
    if caller_identity is FAILURE:
        print('Failed to get caller identity')
        return forbidden('caller_identity')

    reporting_origin = get_reporting_origin(caller_identity)
    if reporting_origin is FAILURE:
        print('Failed to get reporting origin')
        return forbidden('reporting_origin')

    if reported_origin != reporting_origin:
        print('Reported and reporting origin do not match')
        return forbidden('origin_check')

    return (json.dumps({'authorized_domain': reporting_origin}), 200)
