// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <chrono>

using std::get;
using std::make_shared;
using std::string;
using std::to_string;
using std::vector;
using std::chrono::milliseconds;

#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider_no_overrides.h"
#include "pbs/leasable_lock/src/leasable_lock_on_nosql_database.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetDatabaseItemRequest;
using google::scp::core::GetDatabaseItemResponse;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseManagerInterface;
using google::scp::core::LeaseTransitionType;
using google::scp::core::NoSQLDatabaseAttributeName;
using google::scp::core::NoSqlDatabaseKeyValuePair;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::NoSQLDatabaseValidAttributeValueTypes;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::UpsertDatabaseItemRequest;
using google::scp::core::UpsertDatabaseItemResponse;
using google::scp::core::common::TimeProvider;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProviderNoOverrides;
using google::scp::core::test::ResultIs;
using google::scp::pbs::LeasableLockOnNoSQLDatabase;

static constexpr char kPBSPartitionLockTableDefaultName[] =
    "pbs_partition_lock_table";

namespace google::scp::pbs::test {

class LeasableLockOnNoSQLDatabaseTester : public LeasableLockOnNoSQLDatabase {
 public:
  LeasableLockOnNoSQLDatabaseTester()
      : LeasableLockOnNoSQLDatabase(nullptr, {},
                                    kPBSPartitionLockTableDefaultName) {}

  void TestConstructAttributesFromLeaseInfo() {
    LeaseInfoInternal lease;
    lease.lease_expiraton_timestamp_in_milliseconds = milliseconds(123445);
    lease.lease_owner_info.lease_acquirer_id = "214314515515";
    lease.lease_owner_info.service_endpoint_address = "12.1.1.1";

    auto attributes = make_shared<vector<NoSqlDatabaseKeyValuePair>>();
    auto result = ConstructAttributesFromLeaseInfo(lease, attributes);
    EXPECT_SUCCESS(result);
    EXPECT_EQ(attributes->size(), 3);
    EXPECT_EQ(attributes->at(0).attribute_name->compare(
                  kPBSPartitionLockTableLeaseOwnerIdAttributeName),
              0);
    EXPECT_EQ(get<string>(*attributes->at(0).attribute_value),
              lease.lease_owner_info.lease_acquirer_id);
    EXPECT_EQ(attributes->at(1).attribute_name->compare(
                  kPBSLockTableLeaseOwnerServiceEndpointAddressAttributeName),
              0);
    EXPECT_EQ(get<string>(*attributes->at(1).attribute_value),
              lease.lease_owner_info.service_endpoint_address);
    EXPECT_EQ(attributes->at(2).attribute_name->compare(
                  kPBSPartitionLockTableLeaseExpirationTimestampAttributeName),
              0);
    EXPECT_EQ(
        get<string>(*attributes->at(2).attribute_value),
        to_string(lease.lease_expiraton_timestamp_in_milliseconds.count()));
  }

  void TestObtainLeaseInfoFromAttributes() {
    LeaseInfoInternal lease;
    lease.lease_expiraton_timestamp_in_milliseconds = milliseconds(123445);
    lease.lease_owner_info.lease_acquirer_id = "214314515515";
    lease.lease_owner_info.service_endpoint_address = "12.1.1.1";

    auto attributes = make_shared<vector<NoSqlDatabaseKeyValuePair>>();
    auto result = ConstructAttributesFromLeaseInfo(lease, attributes);

    LeaseInfoInternal obtained_lease;
    result = ObtainLeaseInfoFromAttributes(attributes, obtained_lease);
    EXPECT_SUCCESS(result);

    EXPECT_EQ(lease.lease_expiraton_timestamp_in_milliseconds,
              obtained_lease.lease_expiraton_timestamp_in_milliseconds);
    EXPECT_EQ(lease.lease_owner_info.lease_acquirer_id,
              obtained_lease.lease_owner_info.lease_acquirer_id);
    EXPECT_EQ(lease.lease_owner_info.service_endpoint_address,
              obtained_lease.lease_owner_info.service_endpoint_address);
  }

  void TestReadSynchronouslyFromDatabasePassesCorrectContext() {
    LeaseInfoInternal lease;
    lease.lease_expiraton_timestamp_in_milliseconds = milliseconds(444444);
    lease.lease_owner_info.lease_acquirer_id = "389168531658715";
    lease.lease_owner_info.service_endpoint_address = "18.1.1.1";

    auto database = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
    EXPECT_CALL(*database, GetDatabaseItem)
        .WillOnce(
            [=](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
                    get_database_item_context) {
              EXPECT_EQ(get_database_item_context.request->table_name->compare(
                            kPBSPartitionLockTableDefaultName),
                        0);
              EXPECT_EQ(get_database_item_context.request->partition_key
                            ->attribute_name->compare(
                                kPBSPartitionLockTableLockIdKeyName),
                        0);
              EXPECT_EQ(
                  get<string>(*get_database_item_context.request->partition_key
                                   ->attribute_value)
                      .compare(kPBSPartitionLockTableRowKeyForGlobalPartition),
                  0);
              // Sort key is not present
              EXPECT_EQ(get_database_item_context.request->sort_key, nullptr);

              get_database_item_context.response =
                  make_shared<GetDatabaseItemResponse>();
              get_database_item_context.response->table_name =
                  get_database_item_context.request->table_name;
              get_database_item_context.response->partition_key =
                  get_database_item_context.request->partition_key;
              get_database_item_context.response->sort_key =
                  get_database_item_context.request->sort_key;

              get_database_item_context.response->attributes =
                  make_shared<vector<NoSqlDatabaseKeyValuePair>>();
              auto result = ConstructAttributesFromLeaseInfo(
                  lease, get_database_item_context.response->attributes);
              EXPECT_SUCCESS(result);
              EXPECT_EQ(get_database_item_context.response->attributes->size(),
                        3);

              get_database_item_context.result = SuccessExecutionResult();
              get_database_item_context.callback(get_database_item_context);
              return SuccessExecutionResult();
            });
    database_ = database;

    LeaseInfoInternal obtained_lease;
    auto result = ReadLeaseSynchronouslyFromDatabase(obtained_lease);
    EXPECT_SUCCESS(result);

    EXPECT_EQ(lease.lease_expiraton_timestamp_in_milliseconds,
              obtained_lease.lease_expiraton_timestamp_in_milliseconds);
    EXPECT_EQ(lease.lease_owner_info.lease_acquirer_id,
              obtained_lease.lease_owner_info.lease_acquirer_id);
    EXPECT_EQ(lease.lease_owner_info.service_endpoint_address,
              obtained_lease.lease_owner_info.service_endpoint_address);
  }

  void TestReadSynchronouslyFromDatabaseFailsIfRequestExecutionFails() {
    auto database = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
    EXPECT_CALL(*database, GetDatabaseItem)
        .WillOnce(
            [=](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
                    get_database_item_context) {
              get_database_item_context.result =
                  FailureExecutionResult(SC_UNKNOWN);
              get_database_item_context.callback(get_database_item_context);
              return SuccessExecutionResult();
            });
    database_ = database;

    LeaseInfoInternal obtained_lease;
    auto result = ReadLeaseSynchronouslyFromDatabase(obtained_lease);
    EXPECT_NE(result, SuccessExecutionResult());
  }

  void TestWriteLeaseSynchronouslyToDatabasePassesCorrectContext() {
    LeaseInfoInternal prev_lease;
    prev_lease.lease_expiraton_timestamp_in_milliseconds = milliseconds(123445);
    prev_lease.lease_owner_info.lease_acquirer_id = "214314515515";
    prev_lease.lease_owner_info.service_endpoint_address = "12.1.1.1";

    LeaseInfoInternal new_lease;
    new_lease.lease_expiraton_timestamp_in_milliseconds = milliseconds(444444);
    new_lease.lease_owner_info.lease_acquirer_id = "389168531658715";
    new_lease.lease_owner_info.service_endpoint_address = "18.1.1.1";

    auto database = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
    EXPECT_CALL(*database, UpsertDatabaseItem)
        .WillOnce([=](AsyncContext<UpsertDatabaseItemRequest,
                                   UpsertDatabaseItemResponse>&
                          upsert_database_item_context) {
          EXPECT_EQ(upsert_database_item_context.request->table_name->compare(
                        kPBSPartitionLockTableDefaultName),
                    0);
          EXPECT_EQ(upsert_database_item_context.request->partition_key
                        ->attribute_name->compare(
                            kPBSPartitionLockTableLockIdKeyName),
                    0);
          EXPECT_EQ(
              get<string>(*upsert_database_item_context.request->partition_key
                               ->attribute_value)
                  .compare(kPBSPartitionLockTableRowKeyForGlobalPartition),
              0);
          // Sort key is not present
          EXPECT_EQ(upsert_database_item_context.request->sort_key, nullptr);

          auto& attributes = upsert_database_item_context.request->attributes;
          auto& new_attributes =
              upsert_database_item_context.request->new_attributes;
          EXPECT_EQ(attributes->size(), 3);
          EXPECT_EQ(get<string>(*attributes->at(0).attribute_value),
                    prev_lease.lease_owner_info.lease_acquirer_id);
          EXPECT_EQ(get<string>(*attributes->at(1).attribute_value),
                    prev_lease.lease_owner_info.service_endpoint_address);
          EXPECT_EQ(
              get<string>(*attributes->at(2).attribute_value),
              to_string(prev_lease.lease_expiraton_timestamp_in_milliseconds
                            .count()));

          EXPECT_EQ(new_attributes->size(), 3);
          EXPECT_EQ(get<string>(*new_attributes->at(0).attribute_value),
                    new_lease.lease_owner_info.lease_acquirer_id);
          EXPECT_EQ(get<string>(*new_attributes->at(1).attribute_value),
                    new_lease.lease_owner_info.service_endpoint_address);
          EXPECT_EQ(
              get<string>(*new_attributes->at(2).attribute_value),
              to_string(
                  new_lease.lease_expiraton_timestamp_in_milliseconds.count()));

          upsert_database_item_context.result = SuccessExecutionResult();
          upsert_database_item_context.callback(upsert_database_item_context);
          return SuccessExecutionResult();
        });
    database_ = database;

    auto result = WriteLeaseSynchronouslyToDatabase(prev_lease, new_lease);
    EXPECT_SUCCESS(result);
  }

  void TestWriteLeaseSynchronouslyToDatabaseFailsIfRequestExecutionFails() {
    LeaseInfoInternal prev_lease;
    prev_lease.lease_expiraton_timestamp_in_milliseconds = milliseconds(123445);
    prev_lease.lease_owner_info.lease_acquirer_id = "214314515515";
    prev_lease.lease_owner_info.service_endpoint_address = "12.1.1.1";

    LeaseInfoInternal new_lease;
    new_lease.lease_expiraton_timestamp_in_milliseconds = milliseconds(444444);
    new_lease.lease_owner_info.lease_acquirer_id = "389168531658715";
    new_lease.lease_owner_info.service_endpoint_address = "18.1.1.1";

    auto database = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
    EXPECT_CALL(*database, UpsertDatabaseItem)
        .WillOnce([=](AsyncContext<UpsertDatabaseItemRequest,
                                   UpsertDatabaseItemResponse>&
                          upsert_database_item_context) {
          upsert_database_item_context.result =
              FailureExecutionResult(SC_UNKNOWN);
          upsert_database_item_context.callback(upsert_database_item_context);
          return SuccessExecutionResult();
        });
    database_ = database;

    auto result = WriteLeaseSynchronouslyToDatabase(prev_lease, new_lease);
    EXPECT_NE(result, SuccessExecutionResult());
  }
};

TEST(LeasableLockOnNoSQLDatabaseHelpersTest, ConstructAttributesFromLeaseInfo) {
  LeasableLockOnNoSQLDatabaseTester lock;
  lock.TestConstructAttributesFromLeaseInfo();
}

TEST(LeasableLockOnNoSQLDatabaseHelpersTest, ObtainLeaseInfoFromAttributes) {
  LeasableLockOnNoSQLDatabaseTester lock;
  lock.TestObtainLeaseInfoFromAttributes();
}

TEST(LeasableLockOnNoSQLDatabaseHelpersTest,
     TestReadSynchronouslyFromDatabasePassesCorrectContext) {
  LeasableLockOnNoSQLDatabaseTester lock;
  lock.TestReadSynchronouslyFromDatabasePassesCorrectContext();
}

TEST(LeasableLockOnNoSQLDatabaseHelpersTest,
     TestWriteLeaseSynchronouslyToDatabasePassesCorrectContext) {
  LeasableLockOnNoSQLDatabaseTester lock;
  lock.TestWriteLeaseSynchronouslyToDatabasePassesCorrectContext();
}

TEST(LeasableLockOnNoSQLDatabaseHelpersTest,
     TestReadSynchronouslyFromDatabaseFailsIfRequestExecutionFails) {
  LeasableLockOnNoSQLDatabaseTester lock;
  lock.TestReadSynchronouslyFromDatabaseFailsIfRequestExecutionFails();
}

TEST(LeasableLockOnNoSQLDatabaseHelpersTest,
     TestWriteLeaseSynchronouslyToDatabaseFailsIfRequestExecutionFails) {
  LeasableLockOnNoSQLDatabaseTester lock;
  lock.TestWriteLeaseSynchronouslyToDatabaseFailsIfRequestExecutionFails();
}
}  // namespace google::scp::pbs::test
