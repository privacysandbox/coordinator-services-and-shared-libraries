# Prerequisites
* Python 3.10
* Pip3

# Install dependencies

`pip3 install -r requirements.txt`

# Overview

This script provides a declarative way of managing the allowed operator records,
which need to be inserted into the PBS auth table for proper operator authentication
when sending requests to PBS.

There are two main data points that are needed by PBS.
1. Operator identity: Provided as a service account email address, e.g.
   service-account-name@project-id.iam.gserviceaccount.com
2. List of operator's sites: Provided as list of URLs, e.g. [https://google.com, https://google.co.uk]

These values will be provided to the script via a CSV file with two columns, `AccountId` and `AdtechSites` in that order. e.g.
```
AccountId,AdtechSites
service-account1@project-id1.iam.gserviceaccount.com,[https://my-domain1.com]
service-account2@project-id2.iam.gserviceaccount.com,"[https://my-domain2.com, https://my-domain3.com]"
```
**Note:** Remember to enclose the sites list in double quotes in scenarios where the list contains more than one item.

Each row will represent a record that needs to be added or removed from the PBS auth table.<br>
The state of this file is also kept stored in a remote GCS bucket, so when the local state is changed,
a diff is performed against the remote state to find whether a given record should be added, removed or replaced.

The script will show the actions to be performed on the terminal, and will ask for confirmation to continue
before incurring in an actual database operation.

This script needs the following parameters in this order:
1. the gcs bucket name - where the remote state will be stored
2. the allowed operators file path - the local file holding the desired state
3. the auth spanner instance id - the PBS auth Spanner instance ID
4. the auth spanner database id - the PBS auth Spanner database ID
5. the auth table name - the PBS auth Spanner table name

### Example invocation

```
python3 add_allowed_identities.py \
  my-bucket-name \
  /path/to/local/file.csv \
  my-spanner-instance-id \
  my-spanner-database-id \
  my-spanner-table-name
```