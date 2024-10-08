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
import json
import os
import functions_framework
import google.api_core.exceptions
from google.cloud import spanner
from publicsuffixlist import PublicSuffixList

FAILURE = -1

add_failure_stage_context = False
psl = PublicSuffixList()


def forbidden(stage, status_code=403):
  if add_failure_stage_context:
    return json.dumps('authorization forbidden'), status_code, stage
  else:
    return json.dumps('authorization forbidden'), status_code


def get_claimed_identity(headers):
  if 'x-gscp-claimed-identity' not in headers:
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
        token_claims + ('=' * (-len(token_claims) % 4))
    )
    token_claims = json.loads(token_claims)

    if not 'email' in token_claims:
      return FAILURE

    return token_claims['email']
  except Exception as e:
    print(e)
    return FAILURE


def get_adtech_sites(caller_identity):
  """Get the list of adtech sites allowlisted for the caller identity.

  Args:
    caller_identity: The caller identity.

  Returns:
    The list of adtech sites allowlisted for the caller identity or an int error
    message from Spanner.
  """
  try:
    instance_id = os.environ['AUTH_V2_SPANNER_INSTANCE_ID']
    database_id = os.environ['AUTH_V2_SPANNER_DATABASE_ID']
    table_name = os.environ['AUTH_V2_SPANNER_TABLE_NAME']

    client = spanner.Client()

    instance = client.instance(instance_id)
    database = instance.database(database_id)

    with database.snapshot() as snapshot:
      query = (
          'SELECT auth.AdtechSites FROM {} auth WHERE AccountId = @accountId'
          .format(table_name)
      )
      results = snapshot.execute_sql(
          query,
          params={'accountId': caller_identity},
          param_types={'accountId': spanner.param_types.STRING},
      )
      # Get the expected one result or throw
      adtech_sites = results.one()[0]

      if not adtech_sites:
        return 403

    return adtech_sites
  # The call to one() will return exactly one result or throw this NotFound exception.
  except google.api_core.exceptions.NotFound as e:
    print(e)
    return 403
  except google.api_core.exceptions.GoogleAPICallError as e:
    print(e)
    if e.code is not None:
      return e.code
    else:
      return 500
  except Exception as e:
    print(e)
    return 500


def convert_claimed_identity_url_to_site(claimed_identity_url):
  # The PSL library has inconsistent behaviour in retaining the protocol value from the input.
  # Hence we remove any http or https protocols from the input.
  # We force https to be the only allowed protocol. http is not allowed.
  # Thus we add https to the dervied site
  global psl
  claimed_identity_url = claimed_identity_url.replace('http://', '').replace(
      'https://', ''
  )
  claimed_identity_url = claimed_identity_url.split(':')[0]
  claimed_identity_url = claimed_identity_url.split('/')[0]
  private_suffix = psl.privatesuffix(claimed_identity_url)
  return 'https://' + private_suffix


def handle_request(headers):
  claimed_identity_url = get_claimed_identity(headers)
  if claimed_identity_url is FAILURE:
    print('Failed to get claimed identity from headers')
    return forbidden('claimed_identity_url')
  print('Claimed identity Url ', claimed_identity_url)
  derived_site = convert_claimed_identity_url_to_site(claimed_identity_url)
  print('Derived site from claimed identity in the request: ', derived_site)

  caller_identity = get_caller_identity(headers)
  if caller_identity is FAILURE:
    print('Failed to get caller identity')
    return forbidden('caller_identity')

  print('Identified caller identity: ', caller_identity)
  adtech_sites_or_error = get_adtech_sites(caller_identity)
  if isinstance(adtech_sites_or_error, int):
    print(
        'Adtech sites allowlisted for the provided identity are: ',
        adtech_sites_or_error,
    )
    return forbidden('adtech_sites', adtech_sites_or_error)

  print(
      'Adtech sites allowlisted for the provided identity are: ',
      adtech_sites_or_error,
  )
  if derived_site not in adtech_sites_or_error:
    print(
        'Site derived from reported origin is not one of the allowlisted '
        'sites for the adtech'
    )
    return forbidden('auth_check')

  return json.dumps({'authorized_domain': derived_site}), 200


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
  return handle_request(headers)
