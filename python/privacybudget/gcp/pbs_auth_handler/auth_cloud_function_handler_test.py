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
from collections import namedtuple
import json
import os
import unittest
from unittest.mock import MagicMock
from unittest.mock import patch
import python.privacybudget.gcp.pbs_auth_handler.auth_cloud_function_handler as auth_cloud_function_handler
import google.cloud as gc

"""Tests for the PBS auth cloud function handler"""


class AuthCloudFunctionHandlerTest(unittest.TestCase):
    def setUp(self):
        auth_cloud_function_handler.add_failure_stage_context = True

    def test_unauthorized_if_missing_identity_header(self):
        Request = namedtuple('Request', ['headers'])
        request = Request(headers=[])

        ret = auth_cloud_function_handler.function_handler(request)

        self.assertEqual(ret[1], 403)
        self.assertEqual(ret[2], 'reported_origin')

    def test_unauthorized_if_bad_caller_identity(self):
        Request = namedtuple('Request', ['headers'])
        request = Request(headers={'x-gscp-claimed-identity': 'domain.com'})

        ret = auth_cloud_function_handler.function_handler(request)

        self.assertEqual(ret[1], 403)
        self.assertEqual(ret[2], 'caller_identity')

    def test_unauthorized_if_missing_caller_identity(self):
        Request = namedtuple('Request', ['headers'])
        empty_identity = json.dumps({'key': 'value'})
        id_part = base64.b64encode(empty_identity.encode('utf-8'))
        token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

        request = Request(headers={
                          'x-gscp-claimed-identity': 'domain.com',
                          'Authorization': 'bearer {}'.format(token)})

        ret = auth_cloud_function_handler.function_handler(request)

        self.assertEqual(ret[1], 403)
        self.assertEqual(ret[2], 'caller_identity')

    def setup_mock_spanner_result(self, response):
        query_response = gc.QueryResponse()
        query_response.set_response(response)
        snapshot = gc.Snapshot()
        snapshot.set_query_response(query_response)
        database = gc.Database()
        database.set_snapshot(snapshot)
        instance = gc.Instance()
        instance.set_database(database)
        client = gc.Client()
        client.set_instance(instance)
        gc.spanner.set_client(client)

        return {'snapshot': snapshot,
                'database': database,
                'instance': instance,
                'client': client}

    def get_env(self):
        return {
            'REPORTING_ORIGIN_AUTH_SPANNER_INSTANCE_ID': 'my_instance_id',
            'REPORTING_ORIGIN_AUTH_SPANNER_DATABASE_ID': 'my_database_id',
            'REPORTING_ORIGIN_AUTH_SPANNER_TABLE_NAME': 'my_table_name',
            'AUTH_V2_SPANNER_INSTANCE_ID': 'my_instance_v2_id',
            'AUTH_V2_SPANNER_DATABASE_ID': 'my_database_v2_id',
            'AUTH_V2_SPANNER_TABLE_NAME': 'my_table_v2_name',
        }

    def test_unauthorized_if_caller_identity_yields_no_record(self):
        Request = namedtuple('Request', ['headers'])
        empty_identity = json.dumps({'email': 'googler@google.com'})
        id_part = base64.b64encode(empty_identity.encode('utf-8'))
        token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

        request = Request(headers={
                          'x-gscp-claimed-identity': 'domain.com',
                          'Authorization': 'bearer {}'.format(token)})

        # Empty result
        self.setup_mock_spanner_result([])

        ret = None
        with patch.dict('os.environ', self.get_env()):
            ret = auth_cloud_function_handler.function_handler(request)

        self.assertEqual(ret[1], 403)
        self.assertEqual(ret[2], 'reporting_origin')

    def test_unauthorized_if_caller_identity_yields_more_than_one_record(self):
        Request = namedtuple('Request', ['headers'])
        empty_identity = json.dumps({'email': 'googler@google.com'})
        id_part = base64.b64encode(empty_identity.encode('utf-8'))
        token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

        request = Request(headers={
                          'x-gscp-claimed-identity': 'domain.com',
                          'Authorization': 'bearer {}'.format(token)})

        # Return two records
        self.setup_mock_spanner_result(['record1', 'record2'])

        ret = None
        with patch.dict('os.environ', self.get_env()):
            ret = auth_cloud_function_handler.function_handler(request)

        self.assertEqual(ret[1], 403)
        self.assertEqual(ret[2], 'reporting_origin')

    def test_should_query_the_spanner_data_from_environment_vars(self):
        Request = namedtuple('Request', ['headers'])
        empty_identity = json.dumps({'email': 'googler@google.com'})
        id_part = base64.b64encode(empty_identity.encode('utf-8'))
        token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

        request = Request(headers={
                          'x-gscp-claimed-identity': 'domain.com',
                          'Authorization': 'bearer {}'.format(token)})

        # Empty result
        db_mocks = self.setup_mock_spanner_result([])

        with patch.dict('os.environ', self.get_env()):
            auth_cloud_function_handler.function_handler(request)

        self.assertEqual(db_mocks['client'].instance_id, 'my_instance_id')
        self.assertEqual(db_mocks['instance'].database_id, 'my_database_id')
        self.assertEqual(
            db_mocks['snapshot'].query,
            "SELECT ro.ReportingOriginUrl FROM my_table_name ro WHERE AccountId = @accountId")
        self.assertEqual(
            db_mocks['snapshot'].params[0]['accountId'],
            'googler@google.com')
        self.assertEqual(
            db_mocks['snapshot'].param_types['accountId'],
            gc.spanner.param_types.STRING)

    def test_unauthorized_if_reported_origin_does_not_match_reporting_origin(self):
        Request = namedtuple('Request', ['headers'])
        empty_identity = json.dumps({'email': 'googler@google.com'})
        id_part = base64.b64encode(empty_identity.encode('utf-8'))
        token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

        request = Request(headers={
                          'x-gscp-claimed-identity': 'domain.com',
                          'Authorization': 'bearer {}'.format(token)})

        # Return the reporting origin
        self.setup_mock_spanner_result(['reporting-origin.com'])

        ret = None
        with patch.dict('os.environ', self.get_env()):
            ret = auth_cloud_function_handler.function_handler(request)

        self.assertEqual(ret[1], 403)
        self.assertEqual(ret[2], 'origin_check')

    def test_authorized_if_reported_origin_matches_reporting_origin(self):
        Request = namedtuple('Request', ['headers'])
        empty_identity = json.dumps({'email': 'googler@google.com'})
        id_part = base64.b64encode(empty_identity.encode('utf-8'))
        token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

        request = Request(headers={
                          'x-gscp-claimed-identity': 'domain.com',
                          'Authorization': 'bearer {}'.format(token)})

        # Return the reporting origin
        self.setup_mock_spanner_result(['domain.com'])

        ret = None
        with patch.dict('os.environ', self.get_env()):
            ret = auth_cloud_function_handler.function_handler(request)

        self.assertEqual(ret[0], '{"authorized_domain": "domain.com"}')
        self.assertEqual(ret[1], 200)

    # Format
    # @patch(<function_to_mock>, <return_value>)
    # Note that @patch statements are listed in the reverse order.
    # Because of how decorators work, they will be passed in the right order to the function.
    @patch(
        'python.privacybudget.gcp.pbs_auth_handler.auth_cloud_function_handler.handle_request_v1'
    )
    @patch(
        'python.privacybudget.gcp.pbs_auth_handler.auth_cloud_function_handler.handle_request_v2'
    )
    def test_handler_v1_called_if_site_enrollment_flag_absent(
        self, mock_handle_request_v2, mock_handle_request_v1
    ):
      Request = namedtuple('Request', ['headers'])
      request = Request(headers={'x-gscp-claimed-identity': 'domain.com'})

      auth_cloud_function_handler.function_handler(request)

      mock_handle_request_v1.assert_called_once_with(request[0])
      mock_handle_request_v2.assert_not_called()

    # Format
    # @patch(<function_to_mock>, <return_value>)
    # Note that @patch statements are listed in the reverse order.
    # Because of how decorators work, they will be passed in the right order to the function.
    @patch(
        'python.privacybudget.gcp.pbs_auth_handler.auth_cloud_function_handler.handle_request_v1'
    )
    @patch(
        'python.privacybudget.gcp.pbs_auth_handler.auth_cloud_function_handler.handle_request_v2'
    )
    def test_handler_v1_called_if_site_enrollment_flag_false(
        self, mock_handle_request_v2, mock_handle_request_v1
    ):
      Request = namedtuple('Request', ['headers'])
      request = Request(
          headers={
              'x-gscp-claimed-identity': 'domain.com',
              'x-gscp-enable-per-site-enrollment': 'false',
          }
      )

      auth_cloud_function_handler.function_handler(request)

      mock_handle_request_v1.assert_called_once_with(request[0])
      mock_handle_request_v2.assert_not_called()

    # Format
    # @patch(<function_to_mock>, <return_value>)
    # Note that @patch statements are listed in the reverse order.
    # Because of how decorators work, they will be passed in the right order to the function.
    @patch(
        'python.privacybudget.gcp.pbs_auth_handler.auth_cloud_function_handler.handle_request_v1'
    )
    @patch(
        'python.privacybudget.gcp.pbs_auth_handler.auth_cloud_function_handler.handle_request_v2'
    )
    def test_handler_v1_called_if_site_enrollment_flag_true(
        self, mock_handle_request_v2, mock_handle_request_v1
    ):
      Request = namedtuple('Request', ['headers'])
      request = Request(
          headers={
              'x-gscp-claimed-identity': 'domain.com',
              'x-gscp-enable-per-site-enrollment': 'true',
          }
      )

      auth_cloud_function_handler.function_handler(request)

      mock_handle_request_v2.assert_called_once_with(request[0])
      mock_handle_request_v1.assert_not_called()

    def test_should_query_the_spanner_v2_data_from_environment_vars(self):
      Request = namedtuple('Request', ['headers'])
      empty_identity = json.dumps({'email': 'googler@google.com'})
      id_part = base64.b64encode(empty_identity.encode('utf-8'))
      token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

      request = Request(
          headers={
              'x-gscp-claimed-identity': 'domain.com',
              'Authorization': 'bearer {}'.format(token),
              'x-gscp-enable-per-site-enrollment': 'true',
          }
      )

      # Empty result
      db_mocks = self.setup_mock_spanner_result([])

      with patch.dict('os.environ', self.get_env()):
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
      Request = namedtuple('Request', ['headers'])
      empty_identity = json.dumps({'email': 'googler@google.com'})
      id_part = base64.b64encode(empty_identity.encode('utf-8'))
      token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

      request = Request(
          headers={
              'x-gscp-claimed-identity': 'domain.com',
              'Authorization': 'bearer {}'.format(token),
              'x-gscp-enable-per-site-enrollment': 'true',
          }
      )

      # Empty result
      self.setup_mock_spanner_result([])

      ret = None
      with patch.dict('os.environ', self.get_env()):
        ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[1], 403)
      self.assertEqual(ret[2], 'adtech_sites')

    def test_authorized_if_no_protocol_reported_origin_belongs_to_allowlisted_site(
        self,
    ):
      Request = namedtuple('Request', ['headers'])
      empty_identity = json.dumps({'email': 'googler@google.com'})
      id_part = base64.b64encode(empty_identity.encode('utf-8'))
      token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

      request = Request(
          headers={
              'x-gscp-claimed-identity': 'www.domain.com',
              'Authorization': 'bearer {}'.format(token),
              'x-gscp-enable-per-site-enrollment': 'true',
          }
      )

      # Return the reporting origin
      self.setup_mock_spanner_result(['https://domain.com'])

      ret = None
      with patch.dict('os.environ', self.get_env()):
        ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
      self.assertEqual(ret[1], 200)

    def test_authorized_if_http_reported_origin_belongs_to_allowlisted_site(self):
      Request = namedtuple('Request', ['headers'])
      empty_identity = json.dumps({'email': 'googler@google.com'})
      id_part = base64.b64encode(empty_identity.encode('utf-8'))
      token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

      request = Request(
          headers={
              'x-gscp-claimed-identity': 'http://www.domain.com',
              'Authorization': 'bearer {}'.format(token),
              'x-gscp-enable-per-site-enrollment': 'true',
          }
      )

      # Return the reporting origin
      self.setup_mock_spanner_result(['https://domain.com'])

      ret = None
      with patch.dict('os.environ', self.get_env()):
        ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
      self.assertEqual(ret[1], 200)

    def test_authorized_if_https_reported_origin_belongs_to_allowlisted_site(
        self,
    ):
      Request = namedtuple('Request', ['headers'])
      empty_identity = json.dumps({'email': 'googler@google.com'})
      id_part = base64.b64encode(empty_identity.encode('utf-8'))
      token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

      request = Request(
          headers={
              'x-gscp-claimed-identity': 'https://www.domain.com',
              'Authorization': 'bearer {}'.format(token),
              'x-gscp-enable-per-site-enrollment': 'true',
          }
      )

      # Return the reporting origin
      self.setup_mock_spanner_result(['https://domain.com'])

      ret = None
      with patch.dict('os.environ', self.get_env()):
        ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[0], '{"authorized_domain": "https://domain.com"}')
      self.assertEqual(ret[1], 200)

    def test_unauthorized_if_reported_origin_does_not_belong_to_allowlisted_site(
        self,
    ):
      Request = namedtuple('Request', ['headers'])
      empty_identity = json.dumps({'email': 'googler@google.com'})
      id_part = base64.b64encode(empty_identity.encode('utf-8'))
      token = 'HEADER.{}.SIGNATURE'.format(id_part.decode('utf-8'))

      request = Request(
          headers={
              'x-gscp-claimed-identity': 'http://www.domain.co.uk',
              'Authorization': 'bearer {}'.format(token),
              'x-gscp-enable-per-site-enrollment': 'true',
          }
      )

      # Return the reporting origin
      self.setup_mock_spanner_result(['https://domain.com'])

      ret = None
      with patch.dict('os.environ', self.get_env()):
        ret = auth_cloud_function_handler.function_handler(request)

      self.assertEqual(ret[1], 403)
      self.assertEqual(ret[2], 'auth_check')


if __name__ == '__main__':
  unittest.main()
