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
"""Tests for the PBS auth cloud function handler."""

import base64
import dataclasses
import json
import unittest
from unittest import mock
import unittest.mock
import google.api_core.exceptions
import google.cloud as gc
from google.cloud import spanner
from python.privacybudget.gcp.pbs_auth_handler import auth_cloud_function_handler


@dataclasses.dataclass(frozen=True)
class Request:
  headers: dict


class AuthCloudFunctionHandlerTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    auth_cloud_function_handler.add_failure_stage_context = True

  def test_unauthorized_if_missing_identity_header(self):
    request = Request(headers=[])

    ret = auth_cloud_function_handler.function_handler(request)

    self.assertEqual(ret[1], 403)
    self.assertEqual(ret[2], 'claimed_identity_url')

  def test_unauthorized_if_bad_caller_identity(self):
    request = Request(headers={'x-gscp-claimed-identity': 'domain.com'})

    ret = auth_cloud_function_handler.function_handler(request)

    self.assertEqual(ret[1], 403)
    self.assertEqual(ret[2], 'caller_identity')

  def test_unauthorized_if_missing_caller_identity(self):
    empty_identity = json.dumps({'key': 'value'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'domain.com',
            'Authorization': f'bearer {token}',
        }
    )

    ret = auth_cloud_function_handler.function_handler(request)

    self.assertEqual(ret[1], 403)
    self.assertEqual(ret[2], 'caller_identity')

  def _setup_mock_spanner_result(self, response, exception_to_throw=None):
    query_response = gc.QueryResponse()
    query_response.set_response(response)
    snapshot = gc.Snapshot(exception_to_throw)
    snapshot.set_query_response(query_response)
    database = gc.Database()
    database.set_snapshot(snapshot)
    instance = gc.Instance()
    instance.set_database(database)
    client = gc.Client()
    client.set_instance(instance)
    gc.spanner.set_client(client)

    return {
        'snapshot': snapshot,
        'database': database,
        'instance': instance,
        'client': client,
    }

  def _get_env(self):
    return {
        'REPORTING_ORIGIN_AUTH_SPANNER_INSTANCE_ID': 'my_instance_id',
        'REPORTING_ORIGIN_AUTH_SPANNER_DATABASE_ID': 'my_database_id',
        'REPORTING_ORIGIN_AUTH_SPANNER_TABLE_NAME': 'my_table_name',
        'AUTH_V2_SPANNER_INSTANCE_ID': 'my_instance_v2_id',
        'AUTH_V2_SPANNER_DATABASE_ID': 'my_database_v2_id',
        'AUTH_V2_SPANNER_TABLE_NAME': 'my_table_v2_name',
    }

  def test_unauthorized_if_caller_identity_yields_no_record(self):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'domain.com',
            'Authorization': f'bearer {token}',
        }
    )

    # Empty result
    self._setup_mock_spanner_result([])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)
      self.assertEqual(ret[1], 403)
      self.assertEqual(ret[2], 'adtech_sites')

  def test_unauthorized_if_caller_identity_yields_more_than_one_record(self):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'domain.com',
            'Authorization': f'bearer {token}',
        }
    )

    # Return two records
    self._setup_mock_spanner_result(['record1', 'record2'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)
      self.assertEqual(ret[1], 403)
      self.assertEqual(ret[2], 'adtech_sites')

  def test_unauthorized_if_reported_origin_does_not_match_reporting_origin(
      self,
  ):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'domain.com',
            'Authorization': f'bearer {token}',
        }
    )

    # Return the reporting origin
    self._setup_mock_spanner_result(['reporting-origin.com'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[1], 403)
      self.assertEqual(ret[2], 'auth_check')

  def test_should_query_the_spanner_v2_data_from_environment_vars(self):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'domain.com',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Empty result
    db_mocks = self._setup_mock_spanner_result([])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      auth_cloud_function_handler.function_handler(request)

    self.assertEqual(db_mocks['client'].instance_id, 'my_instance_v2_id')
    self.assertEqual(db_mocks['instance'].database_id, 'my_database_v2_id')
    self.assertEqual(
        db_mocks['snapshot'].query,
        'SELECT auth.AdtechSites FROM my_table_v2_name auth WHERE AccountId ='
        ' @accountId',
    )
    self.assertEqual(
        db_mocks['snapshot'].params[0]['accountId'], 'googler@google.com'
    )
    self.assertEqual(
        db_mocks['snapshot'].param_types['accountId'],
        gc.spanner.param_types.STRING,
    )

  def test_unauthorized_if_caller_identity_v2_yields_no_record(self):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'domain.com',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Empty result
    self._setup_mock_spanner_result([])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[1], 403)
      self.assertEqual(ret[2], 'adtech_sites')

  def test_authorized_if_no_protocol_reported_origin_belongs_to_allowlisted_site(
      self,
  ):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'www.domain.com',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Return the reporting origin.
    self._setup_mock_spanner_result(['https://domain.com'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
      self.assertEqual(ret[1], 200)

  def test_authorized_if_http_reported_origin_with_port_belongs_to_allowlisted_site(
      self,
  ):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'http://www.domain.com:8888',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Return the reporting origin.
    self._setup_mock_spanner_result(['https://domain.com'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
      self.assertEqual(ret[1], 200)

  def test_authorized_if_http_reported_origin_with_port_belongs_to_allowlisted_site(
      self,
  ):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'http://www.domain.com:8888',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Return the reporting origin
    self._setup_mock_spanner_result(['https://domain.com'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
      self.assertEqual(ret[1], 200)

  def test_authorized_if_reported_origin_with_trailing_slash_belongs_to_allowlisted_site(
      self,
  ):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'http://www.domain.com/',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Return the reporting origin
    self._setup_mock_spanner_result(['https://domain.com'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)

    self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
    self.assertEqual(ret[1], 200)

  def test_authorized_if_reported_origin_with_port_and_trailing_slash_belongs_to_allowlisted_site(
      self,
  ):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'http://www.domain.com:8080/',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Return the reporting origin
    self._setup_mock_spanner_result(['https://domain.com'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
      self.assertEqual(ret[1], 200)

  def test_authorized_if_https_reported_origin_belongs_to_allowlisted_site(
      self,
  ):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'https://www.domain.com',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Return the reporting origin
    self._setup_mock_spanner_result(['https://domain.com'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
      self.assertEqual(ret[1], 200)

  def test_authorized_if_https_reported_origin_with_port_belongs_to_allowlisted_site(
      self,
  ):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'https://www.domain.com:8888',
            'Authorization': 'bearer {}'.format(token),
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Return the reporting origin
    self._setup_mock_spanner_result(['https://domain.com'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)
      self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
      self.assertEqual(ret[1], 200)

  def test_unauthorized_if_reported_origin_does_not_belong_to_allowlisted_site(
      self,
  ):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'http://www.domain.co.uk',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    # Return the reporting origin
    self._setup_mock_spanner_result(['https://domain.com'])

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[1], 403)
      self.assertEqual(ret[2], 'auth_check')

  def test_spanner_service_unavailable_exception(self):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'www.domain.com',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    self._setup_mock_spanner_result(
        ['https://domain.com'],
        google.api_core.exceptions.ServiceUnavailable('Service unavailable.'),
    )

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)
      self.assertEqual(ret[1], 503)

  def test_spanner_other_exception_return_500(self):
    empty_identity = json.dumps({'email': 'googler@google.com'})
    id_part = base64.b64encode(empty_identity.encode('utf-8'))
    token = f'HEADER.{id_part.decode("utf-8")}.SIGNATURE'

    request = Request(
        headers={
            'x-gscp-claimed-identity': 'www.domain.com',
            'Authorization': f'bearer {token}',
            'x-gscp-enable-per-site-enrollment': 'true',
        }
    )

    self._setup_mock_spanner_result(
        ['https://domain.com'], ValueError,
    )

    with unittest.mock.patch.dict('os.environ', self._get_env()):
      ret = auth_cloud_function_handler.function_handler(request)
      self.assertEqual(ret[1], 500)


if __name__ == '__main__':
  unittest.main()
