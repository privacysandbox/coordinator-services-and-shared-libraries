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

#include "pbs/leasable_lock/src/leasable_lock_on_nosql_database.h"

#include <gtest/gtest.h>

#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider_no_overrides.h"
#include "pbs/leasable_lock/src/error_codes.h"

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
using std::atomic;
using std::forward;
using std::get;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::this_thread::sleep_for;

static constexpr char kPBSPartitionLockTableDefaultName[] =
    "pbs_partition_lock_table";

namespace google::scp::pbs::test {

static const vector<NoSqlDatabaseKeyValuePair> kDummyLockRowAttributes = {
    {make_shared<NoSQLDatabaseAttributeName>(
         kPBSPartitionLockTableLeaseOwnerIdAttributeName),
     make_shared<NoSQLDatabaseValidAttributeValueTypes>("attr1")},
    {make_shared<NoSQLDatabaseAttributeName>(
         kPBSLockTableLeaseOwnerServiceEndpointAddressAttributeName),
     make_shared<NoSQLDatabaseValidAttributeValueTypes>("attr2")},
    {make_shared<NoSQLDatabaseAttributeName>(
         kPBSPartitionLockTableLeaseExpirationTimestampAttributeName),
     make_shared<NoSQLDatabaseValidAttributeValueTypes>("0")}};

static const vector<NoSqlDatabaseKeyValuePair>
    kDummyLockRowAttributesWithLeaseAcquisitionDisallowed = {
        {make_shared<NoSQLDatabaseAttributeName>(
             kPBSPartitionLockTableLeaseOwnerIdAttributeName),
         make_shared<NoSQLDatabaseValidAttributeValueTypes>("attr1")},
        {make_shared<NoSQLDatabaseAttributeName>(
             kPBSLockTableLeaseOwnerServiceEndpointAddressAttributeName),
         make_shared<NoSQLDatabaseValidAttributeValueTypes>("attr2")},
        {make_shared<NoSQLDatabaseAttributeName>(
             kPBSPartitionLockTableLeaseExpirationTimestampAttributeName),
         make_shared<NoSQLDatabaseValidAttributeValueTypes>("0")},
        {make_shared<NoSQLDatabaseAttributeName>(
             kPBSLockTableLeaseAcquisitionDisallowedAttributeName),
         make_shared<NoSQLDatabaseValidAttributeValueTypes>("true")}};

void SetOverridesOnMockNoSQLDatabase(
    shared_ptr<MockNoSQLDatabaseProviderNoOverrides>& mock_db,
    LeaseInfo& lease_info, milliseconds lease_expiration_timestamp) {
  mock_db->get_database_item_mock =
      [=](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
              context) {
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>();
        context.response->attributes->push_back(
            {make_shared<NoSQLDatabaseAttributeName>(
                 kPBSPartitionLockTableLeaseOwnerIdAttributeName),
             make_shared<NoSQLDatabaseValidAttributeValueTypes>(
                 lease_info.lease_acquirer_id)});
        context.response->attributes->push_back(
            {make_shared<NoSQLDatabaseAttributeName>(
                 kPBSLockTableLeaseOwnerServiceEndpointAddressAttributeName),
             make_shared<NoSQLDatabaseValidAttributeValueTypes>(
                 lease_info.service_endpoint_address)});
        context.response->attributes->push_back(
            {make_shared<NoSQLDatabaseAttributeName>(
                 kPBSPartitionLockTableLeaseExpirationTimestampAttributeName),
             make_shared<NoSQLDatabaseValidAttributeValueTypes>(
                 to_string(lease_expiration_timestamp.count()))});

        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      };
  mock_db->upsert_database_item_mock =
      [=](AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
              context) {
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      };
}

template <class... Args>
class LeasableLockOnNoSQLDatabasePrivate : public LeasableLockOnNoSQLDatabase {
 public:
  explicit LeasableLockOnNoSQLDatabasePrivate(Args... args)
      : LeasableLockOnNoSQLDatabase(args...) {}

  void SetCachedCurrentLeaseOwner(LeaseInfo& lease_owner_info,
                                  milliseconds lease_expiration_timestamp) {
    LeaseInfoInternal lease_info;
    lease_info.lease_owner_info = lease_owner_info;
    lease_info.lease_expiraton_timestamp_in_milliseconds =
        lease_expiration_timestamp;
    current_lease_ = lease_info;
  }

  // Should be used only when the current_lease_ is valid
  milliseconds GetCurrentLeaseExpirationTimestamp() {
    if (current_lease_.has_value()) {
      return current_lease_->lease_expiraton_timestamp_in_milliseconds;
    }
    return milliseconds(0);
  }

  bool IsLeaseCached() { return current_lease_.has_value(); }
};

TEST(LeasableLockOnNoSQLDatabaseTest,
     InitializeAndObtainConfiguredLeaseDurationIsSuccessful) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);
  EXPECT_EQ(leasable_lock.GetConfiguredLeaseDurationInMilliseconds(), 1500);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     IsCurrentLeaseOwnerReturnsFalseAfterInitalization) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);
  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);
  auto lease_owner = leasable_lock.GetCurrentLeaseOwnerInfo();
  EXPECT_EQ(lease_owner.has_value(), false);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     ShouldRefreshLeaseIsTrueAfterInitialization) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);
  EXPECT_EQ(leasable_lock.ShouldRefreshLease(), true);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseReadsAndUpsertsLockRowWithReadValueAsPreconditionValue) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  atomic<int> get_call = {0};
  atomic<int> upsert_call = {0};
  mock_db->get_database_item_mock =
      [&](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
              context) {
        get_call++;
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributes);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      };
  mock_db->upsert_database_item_mock =
      [&](AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
              context) {
        upsert_call++;
        EXPECT_EQ(context.request->attributes->size(), 3);
        EXPECT_EQ(*kDummyLockRowAttributes[0].attribute_name,
                  *context.request->attributes->at(0).attribute_name);
        EXPECT_EQ(*kDummyLockRowAttributes[1].attribute_name,
                  *context.request->attributes->at(1).attribute_name);
        EXPECT_EQ(*kDummyLockRowAttributes[2].attribute_name,
                  *context.request->attributes->at(2).attribute_name);
        EXPECT_EQ(
            get<string>(*kDummyLockRowAttributes[0].attribute_value),
            get<string>(*context.request->attributes->at(0).attribute_value));
        EXPECT_EQ(
            get<string>(*kDummyLockRowAttributes[1].attribute_value),
            get<string>(*context.request->attributes->at(1).attribute_value));
        EXPECT_EQ(
            get<string>(*kDummyLockRowAttributes[2].attribute_value),
            get<string>(*context.request->attributes->at(2).attribute_value));
        auto timestamp = stoll(
            get<string>(*context.request->attributes->at(2).attribute_value),
            nullptr);
        EXPECT_EQ(timestamp, 0);

        EXPECT_EQ(context.request->new_attributes->size(), 3);
        EXPECT_EQ(*kDummyLockRowAttributes[0].attribute_name,
                  *context.request->new_attributes->at(0).attribute_name);
        EXPECT_EQ(*kDummyLockRowAttributes[1].attribute_name,
                  *context.request->new_attributes->at(1).attribute_name);
        EXPECT_EQ(*kDummyLockRowAttributes[2].attribute_name,
                  *context.request->new_attributes->at(2).attribute_name);
        EXPECT_EQ(get<string>(
                      *context.request->new_attributes->at(0).attribute_value),
                  lease_acquirer_info_current.lease_acquirer_id);
        EXPECT_EQ(get<string>(
                      *context.request->new_attributes->at(1).attribute_value),
                  lease_acquirer_info_current.service_endpoint_address);
        timestamp =
            stoll(get<string>(
                      *context.request->new_attributes->at(2).attribute_value),
                  nullptr);
        EXPECT_GT(timestamp, 0);
        EXPECT_EQ(context.request->new_attributes->size(), 3);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      };

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), false);
  EXPECT_EQ(leasable_lock.RefreshLease(), SuccessExecutionResult());
  EXPECT_EQ(get_call.load(), 1);
  EXPECT_EQ(upsert_call.load(), 1);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), true);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseFailsIfLeaseAcquisitionIsDisallowed) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  atomic<int> get_call = {0};
  atomic<int> upsert_call = {0};
  mock_db->get_database_item_mock =
      [&](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
              context) {
        get_call++;
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributesWithLeaseAcquisitionDisallowed);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      };
  mock_db->upsert_database_item_mock =
      [&](AsyncContext<UpsertDatabaseItemRequest,
                       UpsertDatabaseItemResponse>&) {
        upsert_call++;
        return SuccessExecutionResult();
      };

  LeasableLockOnNoSQLDatabase leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);
  EXPECT_EQ(leasable_lock.RefreshLease(),
            FailureExecutionResult(
                core::errors::SC_LEASABLE_LOCK_ACQUISITION_DISALLOWED));
  EXPECT_EQ(get_call, 1);
  EXPECT_EQ(upsert_call, 0);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseDoesNotCacheIfReadLockRowRequestFails) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  atomic<int> get_call = {0};
  atomic<int> upsert_call = {0};
  mock_db->get_database_item_mock =
      [&](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
              context) {
        get_call++;
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>();
        context.result = SuccessExecutionResult();
        return FailureExecutionResult(SC_UNKNOWN);
      };
  mock_db->upsert_database_item_mock =
      [&](AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
              context) {
        upsert_call++;
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      };

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), false);
  EXPECT_NE(leasable_lock.RefreshLease(), SuccessExecutionResult());
  EXPECT_EQ(get_call.load(), 1);
  EXPECT_EQ(upsert_call.load(), 0);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), false);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseDoesNotCacheIfReadLockRowFails) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  atomic<int> get_call = {0};
  atomic<int> upsert_call = {0};
  mock_db->get_database_item_mock =
      [&](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
              context) {
        get_call++;
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>();
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.callback(context);
        return SuccessExecutionResult();
      };
  mock_db->upsert_database_item_mock =
      [&](AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
              context) {
        upsert_call++;
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      };

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), false);
  EXPECT_NE(leasable_lock.RefreshLease(), SuccessExecutionResult());
  EXPECT_EQ(get_call.load(), 1);
  EXPECT_EQ(upsert_call.load(), 0);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), false);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseDoesNotCacheIfWriteLockRowRequestFails) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  atomic<int> get_call = {0};
  atomic<int> upsert_call = {0};
  mock_db->get_database_item_mock =
      [&](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
              context) {
        get_call++;
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributes);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      };
  mock_db->upsert_database_item_mock =
      [&](AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
              context) {
        upsert_call++;
        context.result = SuccessExecutionResult();
        return FailureExecutionResult(SC_UNKNOWN);
      };

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), false);
  EXPECT_NE(leasable_lock.RefreshLease(), SuccessExecutionResult());
  EXPECT_EQ(get_call.load(), 1);
  EXPECT_EQ(upsert_call.load(), 1);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), false);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseDoesNotCacheIfWriteLockRowFails) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  atomic<int> get_call = {0};
  atomic<int> upsert_call = {0};
  mock_db->get_database_item_mock =
      [&](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
              context) {
        get_call++;
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributes);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      };
  mock_db->upsert_database_item_mock =
      [&](AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
              context) {
        upsert_call++;
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.callback(context);
        return SuccessExecutionResult();
      };

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), false);
  EXPECT_NE(leasable_lock.RefreshLease(), SuccessExecutionResult());
  EXPECT_EQ(get_call.load(), 1);
  EXPECT_EQ(upsert_call.load(), 1);
  EXPECT_EQ(leasable_lock.IsLeaseCached(), false);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     ShouldRefreshLeaseIsTrueIfOwningLeaseIsExpired) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);

  // Expired lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_current,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() - milliseconds(1)));

  EXPECT_EQ(leasable_lock.ShouldRefreshLease(), true);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     ShouldRefreshLeaseIsFalseIfNonOwningLeaseIsNotExpired) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "1";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);

  LeaseInfo lease_acquirer_info_initial;
  lease_acquirer_info_initial.lease_acquirer_id = "2";
  lease_acquirer_info_initial.service_endpoint_address = "10.2.2.2";

  // Not expired lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_initial,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(100)));

  EXPECT_EQ(leasable_lock.ShouldRefreshLease(), false);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     ShouldRefreshLeaseIsTrueIfNonOwningLeaseIsExpired) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "1";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);

  LeaseInfo lease_acquirer_info_initial;
  lease_acquirer_info_initial.lease_acquirer_id = "2";
  lease_acquirer_info_initial.service_endpoint_address = "10.2.2.2";

  // Expired lease and non owner lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_initial,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() - seconds(1)));

  EXPECT_EQ(leasable_lock.ShouldRefreshLease(), true);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     ShouldRefreshLeaseIsFalseIfOwningLeaseHasNotMetRenewThreshold) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "1";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", seconds(15), 20);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_current,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(6)));

  EXPECT_EQ(leasable_lock.ShouldRefreshLease(), false);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     ShouldRefreshLeaseIsTrueIfOwningLeaseHasMetRenewThreshold) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "1";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", seconds(15), 20);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_current,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(1)));

  EXPECT_EQ(leasable_lock.ShouldRefreshLease(), true);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     IsCurrentLeaseOwnerReturnsTrueIfLeaseOwnerIsCurrent) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_current,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(100)));

  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), true);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     IsCurrentLeaseOwnerReturnsFalseIfLeaseOwnerIsOther) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "1";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);

  LeaseInfo lease_acquirer_info_initial;
  lease_acquirer_info_initial.lease_acquirer_id = "2";
  lease_acquirer_info_initial.service_endpoint_address = "10.2.2.2";

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_initial,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(100)));

  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     IsCurrentLeaseOwnerReturnsFalseIfLeaseOwnerIsCurrentAndExpired) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "123";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", milliseconds(1500), 80);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_current,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() - seconds(1)));

  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseRefreshesTheCachedLeaseForFirstTimeOwnerAndExpired) {
  LeaseInfo lease_acquirer_info_this;
  lease_acquirer_info_this.lease_acquirer_id = "123";
  lease_acquirer_info_this.service_endpoint_address = "10.1.1.1";

  auto current_lease_owner_lease_expiration = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() - seconds(100));
  LeaseInfo lease_acquirer_info_current_owner = lease_acquirer_info_this;

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  SetOverridesOnMockNoSQLDatabase(mock_db, lease_acquirer_info_current_owner,
                                  current_lease_owner_lease_expiration);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_this, kPBSPartitionLockTableDefaultName, "0",
      milliseconds(1500), 80);

  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);

  EXPECT_EQ(leasable_lock.RefreshLease(), SuccessExecutionResult());

  // New lease owner is the other lease owner.
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), true);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_info_this.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_info_this.service_endpoint_address);
}

TEST(
    LeasableLockOnNoSQLDatabaseTest,
    RefreshLeaseRefreshesTheCachedLeaseForFirstTimeNotOwnerAndLeaseNotExpired) {
  LeaseInfo lease_acquirer_info_this;
  lease_acquirer_info_this.lease_acquirer_id = "123";
  lease_acquirer_info_this.service_endpoint_address = "10.1.1.1";

  auto current_lease_owner_lease_expiration = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() + seconds(100));
  LeaseInfo lease_acquirer_info_current_owner;
  lease_acquirer_info_current_owner.lease_acquirer_id = "456";
  lease_acquirer_info_current_owner.service_endpoint_address = "11.11.11.11";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  SetOverridesOnMockNoSQLDatabase(mock_db, lease_acquirer_info_current_owner,
                                  current_lease_owner_lease_expiration);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_this, kPBSPartitionLockTableDefaultName, "0",
      milliseconds(1500), 80);

  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);

  EXPECT_EQ(leasable_lock.RefreshLease(), SuccessExecutionResult());

  // New lease owner is the other lease owner.
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), true);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_info_current_owner.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_info_current_owner.service_endpoint_address);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseRefreshesTheCachedLeaseForFirstTimeNotOwnerAndButExpired) {
  LeaseInfo lease_acquirer_info_this;
  lease_acquirer_info_this.lease_acquirer_id = "123";
  lease_acquirer_info_this.service_endpoint_address = "10.1.1.1";

  auto current_lease_owner_lease_expiration = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() - seconds(1));
  LeaseInfo lease_acquirer_info_current_owner;
  lease_acquirer_info_current_owner.lease_acquirer_id = "456";
  lease_acquirer_info_current_owner.service_endpoint_address = "11.11.11.11";

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  SetOverridesOnMockNoSQLDatabase(mock_db, lease_acquirer_info_current_owner,
                                  current_lease_owner_lease_expiration);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_this, kPBSPartitionLockTableDefaultName, "0",
      milliseconds(1500), 80);

  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);

  EXPECT_EQ(leasable_lock.RefreshLease(), SuccessExecutionResult());

  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), true);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_info_this.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_info_this.service_endpoint_address);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseRefreshesTheCachedLeaseIfOwner) {
  LeaseInfo lease_acquirer_info_this;
  lease_acquirer_info_this.lease_acquirer_id = "123";
  lease_acquirer_info_this.service_endpoint_address = "10.1.1.1";

  auto current_lease_owner_lease_expiration = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() - seconds(1));
  LeaseInfo lease_acquirer_info_current_owner = lease_acquirer_info_this;

  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  SetOverridesOnMockNoSQLDatabase(mock_db, lease_acquirer_info_current_owner,
                                  current_lease_owner_lease_expiration);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current_owner,
      kPBSPartitionLockTableDefaultName, "0", milliseconds(1500), 80);
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_current_owner, current_lease_owner_lease_expiration);

  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);
  EXPECT_EQ(leasable_lock.RefreshLease(), SuccessExecutionResult());

  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), true);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_info_this.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_info_this.service_endpoint_address);
  EXPECT_GT(leasable_lock.GetCurrentLeaseExpirationTimestamp(),
            current_lease_owner_lease_expiration);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseRefreshesTheCachedLeaseIfNonOwnerAndExpired) {
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "1";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  LeaseInfo lease_acquirer_info_initial;
  lease_acquirer_info_initial.lease_acquirer_id = "2";
  lease_acquirer_info_initial.service_endpoint_address = "20.1.1.1";

  milliseconds initial_expired_lease_expiration_timestamp =
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() - milliseconds(1));

  // Initially the database has a lease of other lease acquirer for +10 seconds.
  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  SetOverridesOnMockNoSQLDatabase(mock_db, lease_acquirer_info_initial,
                                  initial_expired_lease_expiration_timestamp);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", seconds(15), 80);
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_info_initial, initial_expired_lease_expiration_timestamp);

  // Current lease acquirer does not own the lease.
  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);

  EXPECT_EQ(leasable_lock.RefreshLease(), SuccessExecutionResult());

  // Current lease acquirer owns the lease.
  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), true);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), true);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_info_current.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_info_current.service_endpoint_address);
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     RefreshLeaseDoesNotRefreshTheCachedLeaseIfNonOwnerAndNotExpired) {
  // Current lease acquirer
  LeaseInfo lease_acquirer_info_current;
  lease_acquirer_info_current.lease_acquirer_id = "1";
  lease_acquirer_info_current.service_endpoint_address = "10.1.1.1";

  // Other lease acquirer
  LeaseInfo lease_acquirer_info_initial;
  lease_acquirer_info_initial.lease_acquirer_id = "2";
  lease_acquirer_info_initial.service_endpoint_address = "20.1.1.1";

  auto initial_lease_expiration_timestamp = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() + seconds(10));

  // Initially the database has a lease of other lease acquirer for +10 seconds.
  auto mock_db = make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  SetOverridesOnMockNoSQLDatabase(mock_db, lease_acquirer_info_initial,
                                  initial_lease_expiration_timestamp);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_db, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName,
      "0", seconds(15), 80);
  leasable_lock.SetCachedCurrentLeaseOwner(lease_acquirer_info_initial,
                                           initial_lease_expiration_timestamp);

  // Current lease acquirer does not own the lease.
  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), true);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_info_initial.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_info_initial.service_endpoint_address);
  auto prev_lease_expiration_timestamp =
      leasable_lock.GetCurrentLeaseExpirationTimestamp();
  EXPECT_EQ(prev_lease_expiration_timestamp,
            initial_lease_expiration_timestamp);

  EXPECT_EQ(leasable_lock.RefreshLease(), SuccessExecutionResult());

  // Current lease acquirer still does not own the lease.
  EXPECT_EQ(leasable_lock.IsCurrentLeaseOwner(), false);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), true);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_info_initial.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_info_initial.service_endpoint_address);
  auto current_lease_expiration_timestamp =
      leasable_lock.GetCurrentLeaseExpirationTimestamp();

  // Lease expiration timestamp is not changed.
  EXPECT_EQ(current_lease_expiration_timestamp,
            prev_lease_expiration_timestamp);
}

template <class... Args>
class LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester
    : public LeasableLockOnNoSQLDatabase {
 public:
  explicit LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester(Args... args)
      : LeasableLockOnNoSQLDatabase(args...) {}

  void LeaseInfoInternalTestIsExpired() {
    LeaseInfo lease_info;
    lease_info.lease_acquirer_id = "1";
    lease_info.service_endpoint_address = "10.1.1.1";

    LeaseInfoInternal lease_info_internal(lease_info);
    EXPECT_TRUE(lease_info_internal.IsExpired());

    lease_info_internal = LeaseInfoInternal(
        lease_info, duration_cast<milliseconds>(
                        TimeProvider::GetWallTimestampInNanoseconds()) +
                        seconds(1));
    EXPECT_FALSE(lease_info_internal.IsExpired());
  }

  void
  LeaseInfoInternalExtendLeaseDurationInMillisecondsFromCurrentTimestamp() {
    LeaseInfo lease_info;
    lease_info.lease_acquirer_id = "1";
    lease_info.service_endpoint_address = "10.1.1.1";

    LeaseInfoInternal lease_info_internal(lease_info);
    EXPECT_TRUE(lease_info_internal.IsExpired());

    lease_info_internal.ExtendLeaseDurationInMillisecondsFromCurrentTimestamp(
        milliseconds(500));
    EXPECT_FALSE(lease_info_internal.IsExpired());

    sleep_for(seconds(1));
    EXPECT_TRUE(lease_info_internal.IsExpired());
  }

  void LeaseInfoInternalTestIsLeaseOwner() {
    LeaseInfo lease_info;
    lease_info.lease_acquirer_id = "1";
    lease_info.service_endpoint_address = "10.1.1.1";

    LeaseInfoInternal lease_info_internal1(lease_info);
    EXPECT_TRUE(lease_info_internal1.IsLeaseOwner("1"));

    LeaseInfoInternal lease_info_internal2(lease_info);
    EXPECT_FALSE(lease_info_internal2.IsLeaseOwner("2"));
  }

  void LeaseInfoInternalTestIsLeaseRenewalRequired() {
    LeaseInfo lease_info;
    lease_info.lease_acquirer_id = "1";
    lease_info.service_endpoint_address = "10.1.1.1";

    LeaseInfoInternal lease_info_internal(lease_info);
    lease_info_internal.ExtendLeaseDurationInMillisecondsFromCurrentTimestamp(
        milliseconds(500));

    EXPECT_FALSE(
        lease_info_internal.IsLeaseRenewalRequired(milliseconds(500), 50));
    EXPECT_FALSE(
        lease_info_internal.IsLeaseRenewalRequired(milliseconds(900), 50));
    EXPECT_TRUE(
        lease_info_internal.IsLeaseRenewalRequired(milliseconds(1100), 50));
  }
};

TEST(LeasableLockOnNoSQLDatabaseTest, LeaseInfoInternalTestIsExpired) {
  LeaseInfo lease_acquirer_info_current;
  LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester leasable_lock(
      nullptr, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName);
  leasable_lock.LeaseInfoInternalTestIsExpired();
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     LeaseInfoInternalExtendLeaseDurationInMillisecondsFromCurrentTimestamp) {
  LeaseInfo lease_acquirer_info_current;
  LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester leasable_lock(
      nullptr, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName);
  leasable_lock
      .LeaseInfoInternalExtendLeaseDurationInMillisecondsFromCurrentTimestamp();
}

TEST(LeasableLockOnNoSQLDatabaseTest, LeaseInfoInternalTestIsLeaseOwner) {
  LeaseInfo lease_acquirer_info_current;
  LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester leasable_lock(
      nullptr, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName);
  leasable_lock.LeaseInfoInternalTestIsLeaseOwner();
}

TEST(LeasableLockOnNoSQLDatabaseTest,
     LeaseInfoInternalTestIsLeaseRenewalRequired) {
  LeaseInfo lease_acquirer_info_current;
  LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester leasable_lock(
      nullptr, lease_acquirer_info_current, kPBSPartitionLockTableDefaultName);
  leasable_lock.LeaseInfoInternalTestIsLeaseRenewalRequired();
}

}  // namespace google::scp::pbs::test
