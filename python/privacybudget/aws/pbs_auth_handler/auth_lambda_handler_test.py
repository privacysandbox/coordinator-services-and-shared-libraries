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

import unittest
import os
from unittest.mock import MagicMock

import auth_lambda_handler
import boto3


class AuthLambdaHandlerTest(unittest.TestCase):

    def test_unauthorized_if_missing_request_context(self):
        ret = auth_lambda_handler.lambda_handler({}, {})
        self.assertEqual(ret['statusCode'], 403)
        self.assertEqual(
            auth_lambda_handler.authorization_failure_stage, "context")

    def test_unauthorized_if_missing_identity(self):
        ret = auth_lambda_handler.lambda_handler({'requestContext': {}}, {})
        self.assertEqual(ret['statusCode'], 403)
        self.assertEqual(
            auth_lambda_handler.authorization_failure_stage, "identity")

    def test_unauthorized_if_missing_user_arn(self):
        ret = auth_lambda_handler.lambda_handler(
            {'requestContext': {'identity': {}}}, {})
        self.assertEqual(ret['statusCode'], 403)
        self.assertEqual(
            auth_lambda_handler.authorization_failure_stage, "userArn")

    def test_unauthorized_if_missing_headers(self):
        ret = auth_lambda_handler.lambda_handler(
            {'requestContext': {'identity': {'userArn': 'arn'}}}, {})
        self.assertEqual(ret['statusCode'], 403)
        self.assertEqual(
            auth_lambda_handler.authorization_failure_stage, "reportedOrigin")

    def test_unauthorized_if_missing_identity_header(self):
        event = {'requestContext': {
            'identity': {'userArn': 'arn'}}, 'headers': {}}
        ret = auth_lambda_handler.lambda_handler(
            event, {})
        self.assertEqual(ret['statusCode'], 403)
        self.assertEqual(
            auth_lambda_handler.authorization_failure_stage, "reportedOrigin")

    def test_should_transform_assume_role_arn(self):
        # The recieved assume role ARN
        assume_role_arn = "arn:aws:sts::123456789012:assumed-role/demo/TestAR"

        event = {'requestContext': {'identity': {'userArn': assume_role_arn}},
                 'headers': {'x-gscp-claimed-identity': 'a.com'}}

        # Mock the call to get the table name from an environment variable
        os.environ = MagicMock(return_value="my-table-name")
        # Mock the calls to boto3
        dynamodb_table_mock = boto3.DynamoDbTableMock()
        dynamodb_table_mock.mock_override_get_item_return({})
        dynamodb_resource_mock = boto3.DynamoDbResourceMock(
            dynamodb_table_mock)
        boto3.resource = MagicMock(return_value=dynamodb_resource_mock)

        auth_lambda_handler.lambda_handler(event, {})

        dynamo_key_used = dynamodb_table_mock.mock_get_last_get_item_key()

        # The transformed ARN
        self.assertEqual(
            dynamo_key_used['iam_role'], 'arn:aws:iam::123456789012:role/demo')

    def test_unauthorized_if_mismatching_reporting_origins(self):
        # The received reported origin
        reported_origin = 'my-reported-origin.com'

        event = {'requestContext': {'identity': {'userArn': 'arn'}},
                 'headers': {'x-gscp-claimed-identity': reported_origin}}

        # Mock the call to get the table name from an environment variable
        os.environ = MagicMock(return_value="my-table-name")
        # Mock the calls to boto3
        dynamodb_table_mock = boto3.DynamoDbTableMock()

        # Item the DB returns
        db_reporting_origin = 'my-mismatching-reporting-origin.com'
        db_item_return = {'Item': {'reporting_origin': db_reporting_origin}}

        dynamodb_table_mock.mock_override_get_item_return(db_item_return)
        dynamodb_resource_mock = boto3.DynamoDbResourceMock(
            dynamodb_table_mock)
        boto3.resource = MagicMock(return_value=dynamodb_resource_mock)

        ret = auth_lambda_handler.lambda_handler(event, {})

        self.assertEqual(ret['statusCode'], 403)
        self.assertEqual(
            auth_lambda_handler.authorization_failure_stage, 'reportingOrigin')

    def test_authorized_if_existing_iam_role_and_matching_reporting_origins(self):
        # The recieved ARN
        user_arn = "arn:aws:iam::123456789012:role/demo"
        reported_origin = 'my-reported-origin.com'

        event = {'requestContext': {'identity': {'userArn': user_arn}},
                 'headers': {'x-gscp-claimed-identity': reported_origin}}

        # Mock the call to get the table name from an environment variable
        os.environ = MagicMock(return_value="my-table-name")
        # Mock the calls to boto3
        dynamodb_table_mock = boto3.DynamoDbTableMock()

        # Item the DB returns
        db_reporting_origin = reported_origin
        db_item_return = {'Item': {'reporting_origin': db_reporting_origin}}

        dynamodb_table_mock.mock_override_get_item_return(db_item_return)
        dynamodb_resource_mock = boto3.DynamoDbResourceMock(
            dynamodb_table_mock)
        boto3.resource = MagicMock(return_value=dynamodb_resource_mock)

        ret = auth_lambda_handler.lambda_handler(event, {})

        self.assertEqual(ret['statusCode'], 200)
        self.assertEqual(
            ret['body'], '{"authorized_domain": "' + reported_origin + '"}')


if __name__ == '__main__':
    unittest.main()
