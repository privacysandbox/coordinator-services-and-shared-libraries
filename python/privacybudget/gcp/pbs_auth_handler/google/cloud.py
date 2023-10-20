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

from enum import Enum

""" Mocks for google cloud classes """

"""Mock query response object"""


class QueryResponse:
    def __init__(self):
        self.reponse = None

    def set_response(self, response):
        self.reponse = response

    def one(self):
        if (len(self.reponse) != 1):
            raise Exception("Throwing to mimic the behavior of one()")
        return self.reponse


"""Mock database snapshot object"""


class Snapshot:
    def __init__(self):
        self.query = None
        self.params = None
        self.param_types = None
        self.query_reponse = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, tb):
        if exc_type is not None:
            return False
        return True

    def set_query_response(self, query_reponse):
        self.query_reponse = query_reponse

    def execute_sql(self, query, params, param_types):
        self.query = query
        self.params = params,
        self.param_types = param_types
        return self.query_reponse


"""Mock database object"""


class Database:
    def __init__(self):
        self.snap = None

    def set_snapshot(self, snap):
        self.snap = snap

    def snapshot(self):
        return self.snap


"""Mock spanner instance object"""


class Instance:
    def __init__(self):
        self.db = None
        self.database_id = None

    def set_database(self, db):
        self.db = db

    def database(self, database_id):
        self.database_id = database_id
        return self.db


"""Mock for database client object"""


class Client:
    def __init__(self):
        self.inst = None
        self.instance_id = None

    def set_instance(self, inst):
        self.inst = inst

    def instance(self, instance_id):
        self.instance_id = instance_id
        return self.inst


"""Mock for spanner object"""


class Spanner:
    class param_types(Enum):
        STRING = 1

    def __init__(self):
        self.client = None

    def set_client(self, client):
        self.client = client

    def Client(self):
        return self.client


"""Global spanner mock object"""
_global_spanner = Spanner()
spanner = _global_spanner
