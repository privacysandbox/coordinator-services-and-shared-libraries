#!/usr/bin/env python

# Copyright 2023 Google LLC
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
from unittest.mock import patch

import python.privacybudget.gcp.operator_onboarding.add_allowed_identities as unit


class AddAllowedIdentitiesTest(unittest.TestCase):
  """Tests for the PBS operator onboarding script"""

  def setUp(self):
    pass

  def test_load_desired_state(self):
    desired_state = unit.load_desired_state_file(
        'python/privacybudget/gcp/operator_onboarding/data/test_allowed_operators.csv'
    )
    lines = desired_state.splitlines(keepends=False)

    self.assertEqual(len(lines), 3)
    # Lines should be sorted
    self.assertEqual(
        lines[0],
        'aaa@project-id3.iam.gserviceaccount.com,[https://domain-three.com]',
    )
    self.assertEqual(
        lines[1],
        'bee@project-id2.iam.gserviceaccount.com,"[https://domain2.com,'
        ' https://domain3.com]"',
    )
    self.assertEqual(
        lines[2],
        'cee@project-id1.iam.gserviceaccount.com,[https://domain-one.com]',
    )

  def test_actions_to_be_taken_result_removal(self):
    remote_state = 'aaa@email.com,[https://domain-a.com]\nbbb@email.com,[https://domain-b.com]\nccc@email.com,[https://domain-c.com]\n'
    # aaa is missing
    local_state = 'bbb@email.com,[https://domain-b.com]\nccc@email.com,[https://domain-c.com]'

    diff = unit.get_diff_from_actual_and_desired_state(
        remote_state, local_state
    )

    removals, additions = unit.get_actions_to_be_taken(diff)

    self.assertEqual(len(removals), 1)
    self.assertEqual(len(additions), 0)
    # aaa should be removed
    self.assertEqual(removals[0], '- aaa@email.com,[https://domain-a.com]')

  def test_actions_to_be_taken_result_addition(self):
    remote_state = 'aaa@email.com,[https://domain-a.com]\nccc@email.com,[https://domain-c.com]\n'
    # bbb is added
    local_state = 'aaa@email.com,[https://domain-a.com]\nbbb@email.com,[https://domain-b.com]\nccc@email.com,[https://domain-c.com]\n'

    diff = unit.get_diff_from_actual_and_desired_state(
        remote_state, local_state
    )

    removals, additions = unit.get_actions_to_be_taken(diff)

    self.assertEqual(len(removals), 0)
    self.assertEqual(len(additions), 1)
    # bbb should be added
    self.assertEqual(additions[0], '+ bbb@email.com,[https://domain-b.com]')

  def test_actions_to_be_taken_result_replacement(self):
    remote_state = 'aaa@email.com,[https://domain-a.com]\nbbb@email.com,[https://domain-b.com]\nccc@email.com,[https://domain-c.com]\n'
    # ccc changed to have 2 items in the list
    local_state = (
        'aaa@email.com,[https://domain-a.com]\nbbb@email.com,[https://domain-b.com]\nccc@email.com,"[https://domain-c.com,'
        ' https://domain-cc.com]"\n'
    )

    diff = unit.get_diff_from_actual_and_desired_state(
        remote_state, local_state
    )

    removals, additions = unit.get_actions_to_be_taken(diff)

    self.assertEqual(len(removals), 1)
    self.assertEqual(len(additions), 1)
    # ccc with single item list should be removed
    self.assertEqual(removals[0], '- ccc@email.com,[https://domain-c.com]')
    # ccc with 2 item list should be added
    self.assertEqual(
        additions[0],
        '+ ccc@email.com,"[https://domain-c.com, https://domain-cc.com]"',
    )

  def test_sql_dml_generation_for_single_removal(self):
    # This is the generated diff
    removals = ['- ccc@email.com,[https://domain-c.com]']

    with patch(
        'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.execute_deletion_statements'
    ) as mock_execute_deletion_statements:
      unit.process_removals(
          removals,
          'ProjectId',
          'SpannerInstanceId',
          'SpannerDbId',
          'SpannerTableName',
      )

      mock_execute_deletion_statements.assert_called_once_with(
          'ProjectId',
          'SpannerInstanceId',
          'SpannerDbId',
          "DELETE FROM SpannerTableName WHERE AccountId IN ('ccc@email.com');",
      )

  def test_sql_dml_generation_for_removals(self):
    # This is the generated diff
    removals = [
        '- ccc@email.com,"[https://domain-c.com, https://domain-cc.com]"',
        '- ddd@email.com,[https://domain-d.com]',
    ]

    with patch(
        'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.execute_deletion_statements'
    ) as mock_execute_deletion_statements:
      unit.process_removals(
          removals,
          'ProjectId',
          'SpannerInstanceId',
          'SpannerDbId',
          'SpannerTableName',
      )

      mock_execute_deletion_statements.assert_called_once_with(
          'ProjectId',
          'SpannerInstanceId',
          'SpannerDbId',
          'DELETE FROM SpannerTableName WHERE AccountId IN'
          " ('ccc@email.com','ddd@email.com');",
      )

  def test_sql_dml_generation_for_single_addition(self):
    # This is the generated diff
    additions = ['+ ddd@email.com,[https://domain-d.com]']

    with patch(
        'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.execute_insertion_statements'
    ) as mock_execute_insertion_statements:
      unit.process_additions(
          additions,
          'ProjectId',
          'SpannerInstanceId',
          'SpannerDbId',
          'SpannerTableName',
      )

      mock_execute_insertion_statements.assert_called_once_with(
          'ProjectId',
          'SpannerInstanceId',
          'SpannerDbId',
          'INSERT INTO SpannerTableName (AccountId,AdtechSites) VALUES'
          " ('ddd@email.com',['https://domain-d.com']);",
      )

  def test_sql_dml_generation_for_additions(self):
    # This is the generated diff
    additions = [
        '+ ccc@email.com,[https://domain-c.com]',
        '+ ddd@email.com,[https://domain-d.com, https://domain-dd.com]',
    ]

    with patch(
        'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.execute_insertion_statements'
    ) as mock_execute_insertion_statements:
      unit.process_additions(
          additions,
          'ProjectId',
          'SpannerInstanceId',
          'SpannerDbId',
          'SpannerTableName',
      )

      mock_execute_insertion_statements.assert_called_once_with(
          'ProjectId',
          'SpannerInstanceId',
          'SpannerDbId',
          'INSERT INTO SpannerTableName (AccountId,AdtechSites) VALUES'
          " ('ccc@email.com',['https://domain-c.com']),('ddd@email.com',['https://domain-d.com',"
          " 'https://domain-dd.com']);",
      )

  # Format
  # @patch(<function_to_mock>, <return_value>)
  # Note that @patch statements are listed in the reverse order.
  # Because of how decorators work, they will be passed in the right order to the function.
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.update_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_additions'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_removals'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken_confirmation'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_diff_from_actual_and_desired_state'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_desired_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.create_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.auth_spanner_resources_exist'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.remote_state_bucket_exists'
  )
  def test_top_level_execution_basic_path_when_remote_state_exists(
      self,
      mock_remote_state_bucket_exists,
      mock_auth_spanner_resources_exist,
      mock_load_remote_state_file,
      mock_create_remote_state_file,
      mock_load_desired_state_file,
      mock_get_diff_from_actual_and_desired_state,
      mock_get_actions_to_be_taken,
      mock_get_actions_to_be_taken_confirmation,
      mock_process_removals,
      mock_process_additions,
      mock_update_remote_state_file,
  ):
    remote_file_content = 'remote file content'
    local_file_content = 'local file content'
    computed_diff = 'diff value'
    removals = 'removals'
    additions = 'additions'

    mock_load_remote_state_file.return_value = remote_file_content
    mock_load_desired_state_file.return_value = local_file_content
    mock_get_diff_from_actual_and_desired_state.return_value = computed_diff
    mock_get_actions_to_be_taken.return_value = (removals, additions)
    mock_get_actions_to_be_taken_confirmation.return_value = True

    state_project = 'state-project'
    db_project = 'db-project'
    bucket_name = 'state-bucket-name'
    remote_file_name = 'remote/file_name'
    local_state_file = '/local/file.csv'
    spanner_instance_id = 'spanner-inst-id'
    spanner_db_id = 'spanner-db-id'
    spanner_table_name = 'spanner-table-name'

    argv = [
        'script_name',
        '--state_project',
        state_project,
        '--db_project',
        db_project,
        bucket_name,
        remote_file_name,
        local_state_file,
        spanner_instance_id,
        spanner_db_id,
        spanner_table_name,
    ]
    self.assertEqual(11, len(argv))

    unit.main_handler(argv)

    mock_load_remote_state_file.assert_called_once_with(
        state_project, bucket_name, remote_file_name
    )

    # Shouldn't have tried to create the remote state file
    # because it already existed.
    mock_create_remote_state_file.assert_not_called()

    mock_load_desired_state_file.assert_called_once_with(local_state_file)
    mock_get_diff_from_actual_and_desired_state.assert_called_once_with(
        remote_file_content, local_file_content
    )
    mock_get_actions_to_be_taken.assert_called_once_with(computed_diff)
    mock_get_actions_to_be_taken_confirmation.assert_called_once_with(
        removals, additions
    )
    mock_process_removals.assert_called_once_with(
        removals,
        db_project,
        spanner_instance_id,
        spanner_db_id,
        spanner_table_name,
    )
    mock_process_additions.assert_called_once_with(
        additions,
        db_project,
        spanner_instance_id,
        spanner_db_id,
        spanner_table_name,
    )
    mock_update_remote_state_file.assert_called_once_with(
        state_project, bucket_name, remote_file_name, local_file_content
    )

  # Format
  # @patch(<function_to_mock>, <return_value>)
  # Note that @patch statements are listed in the reverse order.
  # Because of how decorators work, they will be passed in the right order to the function.
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.update_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_additions'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_removals'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken_confirmation'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken',
      return_value=('removals', 'additions'),
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_diff_from_actual_and_desired_state'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_desired_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.create_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.auth_spanner_resources_exist'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.remote_state_bucket_exists'
  )
  def test_top_level_execution_should_create_remote_file_if_nonexistent(
      self,
      mock_remote_state_bucket_exists,
      mock_auth_spanner_resources_exist,
      mock_load_remote_state_file,
      mock_create_remote_state_file,
      mock_load_desired_state_file,
      mock_get_diff_from_actual_and_desired_state,
      mock_get_actions_to_be_taken,
      mock_get_actions_to_be_taken_confirmation,
      mock_process_removals,
      mock_process_additions,
      mock_update_remote_state_file,
  ):
    # This means the remote file didn't exist
    mock_load_remote_state_file.return_value = None

    bucket_name = 'state-bucket-name'
    remote_file_name = 'remote/file/name'

    argv = [
        'script_name',
        bucket_name,
        remote_file_name,
        '/local/file.csv',
        'spanner-inst-id',
        'spanner-db-id',
        'spanner-table-name',
    ]
    self.assertEqual(7, len(argv))

    unit.main_handler(argv)

    mock_load_remote_state_file.assert_called_once()
    # Should try to create the remote state file.
    mock_create_remote_state_file.assert_called_once_with(
        None, bucket_name, remote_file_name
    )

  # Format
  # @patch(<function_to_mock>, <return_value>)
  # Note that @patch statements are listed in the reverse order.
  # Because of how decorators work, they will be passed in the right order to the function.
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.update_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_additions'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_removals'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken_confirmation'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_diff_from_actual_and_desired_state'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_desired_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.create_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.auth_spanner_resources_exist'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.remote_state_bucket_exists'
  )
  def test_top_level_execution_should_return_early_if_diff_is_empty(
      self,
      mock_remote_state_bucket_exists,
      mock_auth_spanner_resources_exist,
      mock_load_remote_state_file,
      mock_create_remote_state_file,
      mock_load_desired_state_file,
      mock_get_diff_from_actual_and_desired_state,
      mock_get_actions_to_be_taken,
      mock_get_actions_to_be_taken_confirmation,
      mock_process_removals,
      mock_process_additions,
      mock_update_remote_state_file,
  ):
    # Empty diffs
    mock_get_actions_to_be_taken.return_value = ('', '')

    argv = [
        'script_name',
        'state-bucket-name',
        'remote-file-name',
        '/local/file.csv',
        'spanner-inst-id',
        'spanner-db-id',
        'spanner-table-name',
    ]
    self.assertEqual(7, len(argv))

    # Should exit early since diff is empty
    with self.assertRaises(SystemExit):
      unit.main_handler(argv)

    mock_get_actions_to_be_taken.assert_called_once()
    # Because the diff was empty we should exit early and neither trigger
    # DB updates not update the remote state file.
    mock_process_removals.assert_not_called()
    mock_process_additions.assert_not_called()
    mock_update_remote_state_file.assert_not_called()

  # Format
  # @patch(<function_to_mock>, <return_value>)
  # Note that @patch statements are listed in the reverse order.
  # Because of how decorators work, they will be passed in the right order to the function.
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.update_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_additions'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_removals'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken_confirmation'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_diff_from_actual_and_desired_state'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_desired_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.create_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.auth_spanner_resources_exist'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.remote_state_bucket_exists'
  )
  def test_top_level_execution_should_return_early_if_bucket_does_not_exist(
      self,
      mock_remote_state_bucket_exists,
      mock_auth_spanner_resources_exist,
      mock_load_remote_state_file,
      mock_create_remote_state_file,
      mock_load_desired_state_file,
      mock_get_diff_from_actual_and_desired_state,
      mock_get_actions_to_be_taken,
      mock_get_actions_to_be_taken_confirmation,
      mock_process_removals,
      mock_process_additions,
      mock_update_remote_state_file,
  ):
    # Remote state bucket does not exist
    mock_remote_state_bucket_exists.return_value = False

    argv = [
        'script_name',
        'state-bucket-name',
        'remote-file-name',
        '/local/file.csv',
        'spanner-inst-id',
        'spanner-db-id',
        'spanner-table-name',
    ]
    self.assertEqual(7, len(argv))

    # Should exit early since the bucket does not exist
    with self.assertRaises(SystemExit):
      unit.main_handler(argv)

    mock_remote_state_bucket_exists.assert_called_once()
    mock_load_remote_state_file.assert_not_called()
    mock_create_remote_state_file.assert_not_called()
    mock_load_desired_state_file.assert_not_called()
    mock_get_diff_from_actual_and_desired_state.assert_not_called()
    mock_get_actions_to_be_taken.assert_not_called()
    mock_get_actions_to_be_taken_confirmation.assert_not_called()
    mock_process_removals.assert_not_called()
    mock_process_additions.assert_not_called()
    mock_update_remote_state_file.assert_not_called()

  # Format
  # @patch(<function_to_mock>, <return_value>)
  # Note that @patch statements are listed in the reverse order.
  # Because of how decorators work, they will be passed in the right order to the function.
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.update_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_additions'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.process_removals'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken_confirmation'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_actions_to_be_taken'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.get_diff_from_actual_and_desired_state'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_desired_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.create_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.load_remote_state_file'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.auth_spanner_resources_exist'
  )
  @patch(
      'python.privacybudget.gcp.operator_onboarding.add_allowed_identities.remote_state_bucket_exists',
      return_value=True,
  )
  def test_top_level_execution_should_return_early_if_spanner_resources_do_not_exist(
      self,
      mock_remote_state_bucket_exists,
      mock_auth_spanner_resources_exist,
      mock_load_remote_state_file,
      mock_create_remote_state_file,
      mock_load_desired_state_file,
      mock_get_diff_from_actual_and_desired_state,
      mock_get_actions_to_be_taken,
      mock_get_actions_to_be_taken_confirmation,
      mock_process_removals,
      mock_process_additions,
      mock_update_remote_state_file,
  ):
    # Spanner resources do not exist
    mock_auth_spanner_resources_exist.return_value = False

    argv = [
        'script_name',
        'state-bucket-name',
        'remote/file_name',
        '/local/file.csv',
        'spanner-inst-id',
        'spanner-db-id',
        'spanner-table-name',
    ]
    self.assertEqual(7, len(argv))

    # Should exit early since the bucket does not exist
    with self.assertRaises(SystemExit):
      unit.main_handler(argv)

    mock_auth_spanner_resources_exist.assert_called_once()
    mock_load_remote_state_file.assert_not_called()
    mock_create_remote_state_file.assert_not_called()
    mock_load_desired_state_file.assert_not_called()
    mock_get_diff_from_actual_and_desired_state.assert_not_called()
    mock_get_actions_to_be_taken.assert_not_called()
    mock_get_actions_to_be_taken_confirmation.assert_not_called()
    mock_process_removals.assert_not_called()
    mock_process_additions.assert_not_called()
    mock_update_remote_state_file.assert_not_called()

  def test_top_level_execution_should_return_early_if_too_few_arguments(self):
    # There should be 6 args.
    # Here, we're passing one item short so this should exit early
    argv = [
        'script_name',
        'state-bucket-name',
        'remote/file_name',
        '/local/file.csv',
        'spanner-inst-id',
        'spanner-db-id',
    ]
    # This asserts that we're passing the argv list with one missing item.
    # For it to work it should have 6 items, but we're passing only 5.
    self.assertEqual(6, len(argv))

    # Should exit early since the bucket does not exist
    with self.assertRaises(SystemExit):
      unit.main_handler(argv)


if __name__ == '__main__':
  unittest.main()
