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
    const shared_ptr<MockNoSQLDatabaseProviderNoOverrides>&
        mock_nosql_database_provider_,
    const LeaseInfo& lease_info, milliseconds lease_expiration_timestamp) {
  ON_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillByDefault([=](AsyncContext<GetDatabaseItemRequest,
                                      GetDatabaseItemResponse>& context) {
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
      });
  ON_CALL(*mock_nosql_database_provider_, UpsertDatabaseItem)
      .WillByDefault([=](AsyncContext<UpsertDatabaseItemRequest,
                                      UpsertDatabaseItemResponse>& context) {
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });
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

class LeasableLockOnNoSQLDatabaseTest : public ::testing::Test {
 protected:
  LeasableLockOnNoSQLDatabaseTest() {
    // Two different acquirers
    lease_acquirer_1_ = LeaseInfo{"123", "10.1.1.1"};
    lease_acquirer_2_ = LeaseInfo{"456", "10.1.1.2"};
    mock_nosql_database_provider_ =
        make_shared<MockNoSQLDatabaseProviderNoOverrides>();
    nosql_database_provider_ = mock_nosql_database_provider_;
  }

  LeaseInfo lease_acquirer_1_;
  LeaseInfo lease_acquirer_2_;
  size_t lease_renewal_percent_time_left_ = 80;
  milliseconds lease_duration_in_ms_ = milliseconds(1500);
  string leasable_lock_key_ = "0";
  string lease_table_name_ = kPBSPartitionLockTableDefaultName;
  shared_ptr<MockNoSQLDatabaseProviderNoOverrides>
      mock_nosql_database_provider_;
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider_;
};

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       InitializeAndObtainConfiguredLeaseDurationIsSuccessful) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_EQ(leasable_lock.GetConfiguredLeaseDurationInMilliseconds(),
            lease_duration_in_ms_.count());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       IsCurrentLeaseOwnerReturnsFalseAfterInitalization) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueAfterInitialization) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseReadsAndUpsertsLockRowWithReadValueAsPreconditionValue) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributes);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_nosql_database_provider_, UpsertDatabaseItem)
      .WillOnce([&](AsyncContext<UpsertDatabaseItemRequest,
                                 UpsertDatabaseItemResponse>& context) {
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
                  lease_acquirer_1_.lease_acquirer_id);
        EXPECT_EQ(get<string>(
                      *context.request->new_attributes->at(1).attribute_value),
                  lease_acquirer_1_.service_endpoint_address);
        timestamp =
            stoll(get<string>(
                      *context.request->new_attributes->at(2).attribute_value),
                  nullptr);
        EXPECT_GT(timestamp, 0);
        EXPECT_EQ(context.request->new_attributes->size(), 3);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh*/));
  EXPECT_TRUE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseFailsIfLeaseAcquisitionIsDisallowed) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributesWithLeaseAcquisitionDisallowed);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_EQ(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            FailureExecutionResult(
                core::errors::SC_LEASABLE_LOCK_ACQUISITION_DISALLOWED));
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotCacheIfReadLockRowRequestFails) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>();
        context.result = SuccessExecutionResult();
        return FailureExecutionResult(SC_UNKNOWN);
      });
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_NE(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            SuccessExecutionResult());
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotCacheIfReadLockRowFails) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>();
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.callback(context);
        return SuccessExecutionResult();
      });
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_NE(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            SuccessExecutionResult());
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotCacheIfWriteLockRowRequestFails) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributes);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*mock_nosql_database_provider_, UpsertDatabaseItem)
      .WillOnce([&](AsyncContext<UpsertDatabaseItemRequest,
                                 UpsertDatabaseItemResponse>& context) {
        context.result = SuccessExecutionResult();
        return FailureExecutionResult(SC_UNKNOWN);
      });

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_NE(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            SuccessExecutionResult());
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotCacheIfWriteLockRowFails) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            make_shared<vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributes);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_nosql_database_provider_, UpsertDatabaseItem)
      .WillOnce([&](AsyncContext<UpsertDatabaseItemRequest,
                                 UpsertDatabaseItemResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.callback(context);
        return SuccessExecutionResult();
      });
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_NE(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            SuccessExecutionResult());
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueIfOwningLeaseIsExpired) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  // Expired lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() - milliseconds(1)));
  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsFalseIfNonOwningLeaseIsNotExpired) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  // Not expired lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(100)));

  EXPECT_FALSE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueIfNonOwningLeaseIsExpired) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  // Expired lease and non owner lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() - seconds(1)));

  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsFalseIfOwningLeaseHasNotMetRenewThreshold) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(6)));

  EXPECT_FALSE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueIfOwningLeaseHasMetRenewThreshold) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(1)));

  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueIfNonOwningLeaseHasStaleCachedLease) {
  auto lease_duration = seconds(15);
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration, lease_renewal_percent_time_left_);

  // Non expired lease and non owner lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(2)));

  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsFalseIfNonOwningLeaseHasFreshCachedLease) {
  auto lease_duration = seconds(15);
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration, lease_renewal_percent_time_left_);

  // Non expired lease and non owner lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(9)));

  EXPECT_FALSE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       IsCurrentLeaseOwnerReturnsTrueIfLeaseOwnerIsCurrent) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(100)));

  EXPECT_TRUE(leasable_lock.IsCurrentLeaseOwner());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       IsCurrentLeaseOwnerReturnsFalseIfLeaseOwnerIsOther) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() + seconds(100)));

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       IsCurrentLeaseOwnerReturnsFalseIfLeaseOwnerIsCurrentAndExpired) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_,
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() - seconds(1)));

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseRefreshesTheCachedLeaseForFirstTimeOwnerAndExpired) {
  auto current_lease_owner_lease_expiration = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() - seconds(100));
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_1_,
                                  current_lease_owner_lease_expiration);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  // New lease owner is the other lease owner.
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_1_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_1_.service_endpoint_address);
}

TEST_F(
    LeasableLockOnNoSQLDatabaseTest,
    RefreshLeaseRefreshesTheCachedLeaseForFirstTimeNotOwnerAndLeaseNotExpired) {
  auto current_lease_owner_lease_expiration = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() + seconds(100));
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  current_lease_owner_lease_expiration);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  // New lease owner is the other lease owner.
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_2_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_2_.service_endpoint_address);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseRefreshesTheCachedLeaseForFirstTimeNotOwnerAndButExpired) {
  auto current_lease_owner_lease_expiration = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() - seconds(1));
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  current_lease_owner_lease_expiration);
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_1_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_1_.service_endpoint_address);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseRefreshesTheCachedLeaseIfOwner) {
  auto current_lease_owner_lease_expiration = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() - seconds(1));
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_1_,
                                  current_lease_owner_lease_expiration);
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_, current_lease_owner_lease_expiration);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);
  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_1_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_1_.service_endpoint_address);
  EXPECT_GT(leasable_lock.GetCurrentLeaseExpirationTimestamp(),
            current_lease_owner_lease_expiration);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseRefreshesTheCachedLeaseIfNonOwnerAndExpired) {
  milliseconds initial_expired_lease_expiration_timestamp =
      duration_cast<milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() - milliseconds(1));
  // Initially the database has a lease of other lease acquirer for +10
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  initial_expired_lease_expiration_timestamp);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_, initial_expired_lease_expiration_timestamp);

  // Current lease acquirer does not own the lease.
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  // Current lease acquirer owns the lease.
  EXPECT_TRUE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_1_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_1_.service_endpoint_address);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotRefreshTheCachedLeaseIfNonOwnerAndNotExpired) {
  auto initial_lease_expiration_timestamp = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() + seconds(10));

  // Initially the database has a lease of other lease acquirer for +10
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  initial_lease_expiration_timestamp);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  leasable_lock.SetCachedCurrentLeaseOwner(lease_acquirer_2_,
                                           initial_lease_expiration_timestamp);

  // Current lease acquirer does not own the lease.
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_2_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_2_.service_endpoint_address);
  auto prev_lease_expiration_timestamp =
      leasable_lock.GetCurrentLeaseExpirationTimestamp();
  EXPECT_EQ(prev_lease_expiration_timestamp,
            initial_lease_expiration_timestamp);

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  // Current lease acquirer still does not own the lease.
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_2_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_2_.service_endpoint_address);
  auto current_lease_expiration_timestamp =
      leasable_lock.GetCurrentLeaseExpirationTimestamp();

  // Lease expiration timestamp is not changed.
  EXPECT_EQ(current_lease_expiration_timestamp,
            prev_lease_expiration_timestamp);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest, LeaseIsNotWrittenIfReadOnlyIsTrue) {
  LeaseInfo lease_acquirer_2_ = {"2", "2.1.1.1"};
  LeaseInfo lease_acquirer_info_next = {"1", "10.1.1.1"};
  milliseconds initial_lease_expiration_timestamp = duration_cast<milliseconds>(
      TimeProvider::GetWallTimestampInNanoseconds() + seconds(100));
  // Initially the database has a lease of other lease acquirer for +10
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  initial_lease_expiration_timestamp);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_info_next,
      kPBSPartitionLockTableDefaultName, "0", seconds(15), 80);
  leasable_lock.SetCachedCurrentLeaseOwner(lease_acquirer_2_,
                                           initial_lease_expiration_timestamp);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(true /* is_read_only_lease_refresh */));
  // Lease cannot be owned due to readonly refresh. Lease holder will be
  // 'lease_acquirer_2_'
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_2_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_2_.service_endpoint_address);
}

}  // namespace google::scp::pbs::test
