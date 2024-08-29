#!/usr/bin/env python

# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
from difflib import Differ
import sys
from google.cloud import spanner, storage
import pandas

IDENTITY_COLUMN_NAME = 'AccountId'
ADTECH_SITES_COLUMN_NAME = 'AdtechSites'
COLOR_GREEN = '\033[92m'
COLOR_RED = '\033[91m'
COLOR_END = '\033[0m'


def load_desired_state_file(file_path):
  """Load the local CSV file containing the desired state.

  Sort the rows to be able to properly compare states.
  """
  csv_data = None
  try:
    csv_data = pandas.read_csv(file_path)
  except pandas.errors.EmptyDataError:
    print(
        '\nThe local CSV file is completely empty. Please make sure it at least'
        ' contains the header columns.'
    )
    return None

  actual_column_names = list(csv_data.columns.values)
  expected_column_names = [IDENTITY_COLUMN_NAME, ADTECH_SITES_COLUMN_NAME]

  if actual_column_names != expected_column_names:
    print('\nUnexpected column names at beginning of CSV.')
    print('Expected: ' + str(expected_column_names))
    print('Got: ' + str(actual_column_names))
    return None

  csv_data.sort_values(
      [IDENTITY_COLUMN_NAME, ADTECH_SITES_COLUMN_NAME],
      axis=0,
      ascending=[True, True],
      inplace=True,
  )
  return csv_data.to_csv(header=False, index=False)


def remote_state_bucket_exists(project, bucket_name):
  """Check whether the bucket exists."""
  client = storage.Client(project)
  bucket = client.bucket(bucket_name)

  if not bucket.exists():
    print('\nThe bucket {} does not exist.'.format(bucket_name))
    return False
  return True


def load_remote_state_file(project, bucket_name, file_name):
  """Load the remote state file stored in the GCS bucket."""
  client = storage.Client(project)
  bucket = client.get_bucket(bucket_name)

  blob = bucket.get_blob(file_name)
  # If the blob doesn't exist just return None
  if blob is None:
    return None
  return blob.download_as_text()


def create_remote_state_file(project, bucket_name, file_name):
  """Create an empty remote state file in the GCS bucket."""
  client = storage.Client(project)
  bucket = client.get_bucket(bucket_name)
  blob = bucket.blob(file_name)
  with blob.open('w') as state_file:
    state_file.write('')


def auth_spanner_resources_exist(project, instance_id, database_id, table_name):
  spanner_client = spanner.Client(project)
  instance = spanner_client.instance(instance_id)
  if not instance.exists():
    print('\nSpanner instance {} does not exist.'.format(instance_id))
    return False
  database = instance.database(database_id)
  if not database.exists():
    print('\nSpanner database {} does not exist.'.format(database_id))
    return False
  table = database.table(table_name)
  if not table.exists():
    print('\nSpanner table {} does not exist.'.format(table_name))
    return False
  return True


def update_remote_state_file(project, bucket_name, file_name, file_contents):
  """Update the remote state file by replacing it.

  1. Rename the existing state file to keep it as a backup. 2. Create a new
  state file with the desired state 3. Delete the backup file.
  """
  client = storage.Client(project)
  bucket = client.get_bucket(bucket_name)

  # Rename the existing state file as backup
  existing_blob = bucket.get_blob(file_name)
  renamed_blob = bucket.rename_blob(existing_blob, file_name + '.backup')

  # Create a new file with the state file name and write new state
  new_blob = bucket.blob(file_name)
  with new_blob.open('w') as state_file:
    state_file.write(file_contents)

  # Delete backup file
  renamed_blob.delete()


def get_actions_to_be_taken(diff):
  """Get the lists of records to add and/or to remove."""
  removals = []
  additions = []
  for item in diff:
    if item.startswith('-'):
      removals.append(item)
    elif item.startswith('+'):
      additions.append(item)
  return removals, additions


def get_actions_to_be_taken_confirmation(removals, additions):
  """Report the actions to be taken and prompt for confirmation."""
  if len(removals) > 0:
    print('\nThe following items will be REMOVED:')
  for r in removals:
    print(COLOR_RED + r.strip() + COLOR_END)

  if len(additions) > 0:
    print('\nThe following items will be ADDED:')
  for a in additions:
    print(COLOR_GREEN + a.strip() + COLOR_END)

  selection = input('\nConfirm these actions [yes/no]:')
  return selection == 'yes'


def execute_deletion_statements(
    project, instance_id, database_id, delete_statement
):
  """Execute the deletion statement against the database."""
  spanner_client = spanner.Client(project)
  instance = spanner_client.instance(instance_id)
  database = instance.database(database_id)

  def delete_items(transaction):
    transaction.execute_update(delete_statement)

  database.run_in_transaction(delete_items)


def process_removals(removals, project, instance_id, database_id, table_name):
  """Process the records that need to be deleted from the database.

  1. Build the delete statement based on the input. 2. Execute the DB
  transaction.
  """
  if len(removals) == 0:
    return

  delete_statement = 'DELETE FROM {} WHERE {} IN ('.format(
      table_name, IDENTITY_COLUMN_NAME
  )
  for index, r in enumerate(removals):
    # The removal item looks like a diff line e.g.
    # - email@domain.com,"[url1.com, url2.com]"
    # So we get rid of the '- ' and split it by the first comma
    row_data = r.strip().split(' ')[1].split(',', 1)
    delete_statement += "'{}'".format(row_data[0])
    if index < len(removals) - 1:
      delete_statement += ','
  delete_statement += ');'

  # The final DML is of the form:
  # DELETE FROM <table_name> WHERE <id_col> IN ('value1', 'value2', ...)
  print(
      COLOR_RED,
      'DELETE Statement to be executed:\n',
      delete_statement,
      COLOR_END,
  )

  execute_deletion_statements(
      project, instance_id, database_id, delete_statement
  )


def execute_insertion_statements(
    project, instance_id, database_id, insert_statement
):
  """Execute the insertion statement against the database."""
  spanner_client = spanner.Client(project)
  instance = spanner_client.instance(instance_id)
  database = instance.database(database_id)

  def insert_items(transaction):
    transaction.execute_update(insert_statement)

  database.run_in_transaction(insert_items)


def process_additions(additions, project, instance_id, database_id, table_name):
  """Process the records that need to be added to the database.

  1. Build the insert statement based on the input.
  2. Execute the DB transaction.
  """
  if len(additions) == 0:
    return

  insert_statement = 'INSERT INTO {} ({},{}) VALUES '.format(
      table_name, IDENTITY_COLUMN_NAME, ADTECH_SITES_COLUMN_NAME
  )
  for index, r in enumerate(additions):
    # The addition item looks like a diff line e.g.
    # + email@domain.com,[url.com]
    # or
    # + email@domain.com,"[url.com, url2.com]"
    # We first get rid of the '+ ' and work our way through the remaining data.
    # We split on the first comma to separate out the 1st and 2nd column values.
    # We strip the leading and trailing quotes and braces and split on comma.
    # This converts the stringified list to an actual python list object
    row_data = r.strip().split(' ', 1)[1].split(',', 1)
    sites_array = row_data[1].strip('"\'][').split(',')
    sites_array = [item.strip() for item in sites_array]
    insert_statement += "('{}',{})".format(row_data[0], sites_array)
    if index < len(additions) - 1:
      insert_statement += ','
  insert_statement += ';'

  # The final query is of the form:
  # INSERT INTO <table_name> (<id_col>, <ro_col>) VALUES ('val1', 'val1-2'), ('val2', 'val2-2), ...

  print(
      COLOR_GREEN,
      'INSERT Statement to be executed:\n',
      insert_statement,
      COLOR_END,
  )
  execute_insertion_statements(
      project, instance_id, database_id, insert_statement
  )


def get_diff_from_actual_and_desired_state(
    current_state_file, desired_state_file
):
  """Compute the diff between the contents of the current (remote) state file and

  the local (desired) state file.
  """
  differ = Differ()
  return differ.compare(
      current_state_file.splitlines(keepends=False),
      desired_state_file.splitlines(keepends=False),
  )


def main_handler(argv):
  parser = argparse.ArgumentParser(
      epilog=(
          'Example Usage:            python3 add_allowed_identities.py         '
          '    my-bucket-name             my-env/allowed_ids            '
          ' /path/to/local/file.csv             my-spanner-instance-id         '
          '    my-spanner-database-id             my-spanner-table-name        '
      )
  )
  parser.add_argument(
      'remote_state_bucket_name',
      help='where the remote state will be stored',
      type=str,
  )
  parser.add_argument(
      'remote_state_file_name',
      help=(
          'the remote state file name - including any prefix e.g.'
          ' auth_states/prod/allowed_ids'
      ),
      type=str,
  )
  parser.add_argument(
      'desired_state_file_path',
      help=(
          'the allowed operators file path - the local file holding the desired'
          ' state'
      ),
      type=str,
  )
  parser.add_argument(
      'auth_spanner_instance_id',
      help='the auth spanner instance id - the PBS auth Spanner instance ID',
      type=str,
  )
  parser.add_argument(
      'auth_spanner_database_id',
      help='the auth spanner database id - the PBS auth Spanner database ID',
      type=str,
  )
  parser.add_argument(
      'auth_table_name',
      help='the auth table name - the PBS auth Spanner table name',
      type=str,
  )
  parser.add_argument(
      '--state_project',
      help=(
          'GCP project containing bucket containing auth file (defaults to'
          ' project set by gcloud)'
      ),
      type=str,
  )
  parser.add_argument(
      '--db_project',
      help=(
          'GCP project containing PBS Auth spanner database (defaults to'
          ' project set by gcloud)'
      ),
      type=str,
  )

  args = parser.parse_args(argv[1:])
  if not remote_state_bucket_exists(
      args.state_project, args.remote_state_bucket_name
  ):
    print('Exiting...')
    sys.exit(-1)
  if not auth_spanner_resources_exist(
      args.db_project,
      args.auth_spanner_instance_id,
      args.auth_spanner_database_id,
      args.auth_table_name,
  ):
    print('Exiting...')
    sys.exit(-1)

  # 1. Load the remote state file from the GCS bucket.
  # Create it if it doesn't exist.
  current_state_file = load_remote_state_file(
      args.state_project,
      args.remote_state_bucket_name,
      args.remote_state_file_name,
  )
  if current_state_file is None:
    current_state_file = ''
    create_remote_state_file(
        args.state_project,
        args.remote_state_bucket_name,
        args.remote_state_file_name,
    )

  # 2. Load the local state file as the desired state
  desired_state_file = load_desired_state_file(args.desired_state_file_path)
  if desired_state_file is None:
    print('Exiting...')
    sys.exit(-1)

  # 3. Compute the diff between the remote and local states
  diff = get_diff_from_actual_and_desired_state(
      current_state_file, desired_state_file
  )

  # 4. If the diff is empty, there is nothing to do
  removals, additions = get_actions_to_be_taken(diff)
  if len(removals) == 0 and len(additions) == 0:
    print('\nNothing to do. Exiting...')
    sys.exit(0)

  # 5. Report the actions that will be taken, i.e. insertions and deletions,
  # and prompt for confirmation
  proceed = get_actions_to_be_taken_confirmation(removals, additions)
  if not proceed:
    print('\nWill not proceed. Exiting...')
    sys.exit(0)

  # 6. Execute the actions against the DB
  process_removals(
      removals,
      args.db_project,
      args.auth_spanner_instance_id,
      args.auth_spanner_database_id,
      args.auth_table_name,
  )
  process_additions(
      additions,
      args.db_project,
      args.auth_spanner_instance_id,
      args.auth_spanner_database_id,
      args.auth_table_name,
  )

  # 7. Update the remote state with the new state
  update_remote_state_file(
      args.state_project,
      args.remote_state_bucket_name,
      args.remote_state_file_name,
      desired_state_file,
  )

  print('\nDone :)')


if __name__ == '__main__':
  main_handler(sys.argv)
