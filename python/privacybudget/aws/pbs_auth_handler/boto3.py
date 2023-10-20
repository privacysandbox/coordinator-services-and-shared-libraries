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

"""This is a boto 3 local mock module"""


class DynamoDbTableMock:
    def __init__(self):
        self.item = None
        self.get_item_key = None

    """Set the value that the get_item function will return"""

    def mock_override_get_item_return(self, item):
        self.item = item

    """Get the last Key that get_item was called with"""

    def mock_get_last_get_item_key(self):
        return self.get_item_key

    """Public boto 3 function called from actual code"""

    def get_item(self, Key={}):
        self.get_item_key = Key
        return self.item


class DynamoDbResourceMock:
    def __init__(self, table_mock):
        self.table_mock = table_mock

    def Table(self, table_name):
        return self.table_mock


"""Public boto 3 function called from actual code"""


def resource(name):
    pass
