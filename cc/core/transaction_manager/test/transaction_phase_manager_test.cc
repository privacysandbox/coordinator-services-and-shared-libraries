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

#include "core/transaction_manager/src/transaction_phase_manager.h"

#include <gtest/gtest.h>

#include "public/core/interface/execution_result.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::transaction_manager::TransactionPhase;

namespace google::scp::core::test {
TEST(TransactionPhaseManagerTests, UnknownPhase) {
  TransactionPhaseManager transaction_phase_manager;
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Unknown, SuccessExecutionResult()),
            TransactionPhase::Aborted);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Unknown, FailureExecutionResult(1)),
            TransactionPhase::Aborted);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Unknown, RetryExecutionResult(1)),
            TransactionPhase::Aborted);
}

TEST(TransactionPhaseManagerTests, NotStartedPhase) {
  TransactionPhaseManager transaction_phase_manager;
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::NotStarted, SuccessExecutionResult()),
            TransactionPhase::Begin);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::NotStarted, FailureExecutionResult(1)),
            TransactionPhase::End);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::NotStarted, RetryExecutionResult(1)),
            TransactionPhase::NotStarted);
}

TEST(TransactionPhaseManagerTests, BeginPhase) {
  TransactionPhaseManager transaction_phase_manager;
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Begin, SuccessExecutionResult()),
            TransactionPhase::Prepare);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Begin, FailureExecutionResult(1)),
            TransactionPhase::Aborted);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Begin, RetryExecutionResult(1)),
            TransactionPhase::Begin);
}

TEST(TransactionPhaseManagerTests, PreparePhase) {
  TransactionPhaseManager transaction_phase_manager;
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Prepare, SuccessExecutionResult()),
            TransactionPhase::Commit);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Prepare, FailureExecutionResult(1)),
            TransactionPhase::Aborted);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Prepare, RetryExecutionResult(1)),
            TransactionPhase::Prepare);
}

TEST(TransactionPhaseManagerTests, CommitPhase) {
  TransactionPhaseManager transaction_phase_manager;
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Commit, SuccessExecutionResult()),
            TransactionPhase::CommitNotify);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Commit, FailureExecutionResult(1)),
            TransactionPhase::AbortNotify);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Commit, RetryExecutionResult(1)),
            TransactionPhase::Commit);
}

TEST(TransactionPhaseManagerTests, CommitNotifyPhase) {
  TransactionPhaseManager transaction_phase_manager;
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::CommitNotify, SuccessExecutionResult()),
            TransactionPhase::Committed);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::CommitNotify, FailureExecutionResult(1)),
            TransactionPhase::Unknown);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::CommitNotify, RetryExecutionResult(1)),
            TransactionPhase::CommitNotify);
}

TEST(TransactionPhaseManagerTests, CommittedPhase) {
  TransactionPhaseManager transaction_phase_manager;
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Committed, SuccessExecutionResult()),
            TransactionPhase::End);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Committed, FailureExecutionResult(1)),
            TransactionPhase::Unknown);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Committed, RetryExecutionResult(1)),
            TransactionPhase::Committed);
}

TEST(TransactionPhaseManagerTests, AbortNotifyPhase) {
  TransactionPhaseManager transaction_phase_manager;
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::AbortNotify, SuccessExecutionResult()),
            TransactionPhase::Aborted);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::AbortNotify, FailureExecutionResult(1)),
            TransactionPhase::Unknown);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::AbortNotify, RetryExecutionResult(1)),
            TransactionPhase::AbortNotify);
}

TEST(TransactionPhaseManagerTests, AbortedPhase) {
  TransactionPhaseManager transaction_phase_manager;
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Aborted, SuccessExecutionResult()),
            TransactionPhase::End);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Aborted, FailureExecutionResult(1)),
            TransactionPhase::Unknown);
  EXPECT_EQ(transaction_phase_manager.ProceedToNextPhase(
                TransactionPhase::Aborted, RetryExecutionResult(1)),
            TransactionPhase::Aborted);
}

TEST(TransactionPhaseManagerTests, CanProceedToAbortAtPhase) {
  EXPECT_FALSE(TransactionPhaseManager::CanProceedToAbortAtPhase(
      TransactionPhase::NotStarted));
  EXPECT_TRUE(TransactionPhaseManager::CanProceedToAbortAtPhase(
      TransactionPhase::Begin));
  EXPECT_TRUE(TransactionPhaseManager::CanProceedToAbortAtPhase(
      TransactionPhase::Prepare));
  EXPECT_TRUE(TransactionPhaseManager::CanProceedToAbortAtPhase(
      TransactionPhase::Commit));
  EXPECT_TRUE(TransactionPhaseManager::CanProceedToAbortAtPhase(
      TransactionPhase::CommitNotify));
  EXPECT_FALSE(TransactionPhaseManager::CanProceedToAbortAtPhase(
      TransactionPhase::Committed));
  EXPECT_TRUE(TransactionPhaseManager::CanProceedToAbortAtPhase(
      TransactionPhase::AbortNotify));
  EXPECT_FALSE(TransactionPhaseManager::CanProceedToAbortAtPhase(
      TransactionPhase::Aborted));
  EXPECT_FALSE(
      TransactionPhaseManager::CanProceedToAbortAtPhase(TransactionPhase::End));
}

TEST(TransactionPhaseManagerTests, CanProceedToEndAtPhase) {
  EXPECT_FALSE(TransactionPhaseManager::CanProceedToEndAtPhase(
      TransactionPhase::NotStarted));
  EXPECT_TRUE(
      TransactionPhaseManager::CanProceedToEndAtPhase(TransactionPhase::Begin));
  EXPECT_TRUE(TransactionPhaseManager::CanProceedToEndAtPhase(
      TransactionPhase::Prepare));
  EXPECT_FALSE(TransactionPhaseManager::CanProceedToEndAtPhase(
      TransactionPhase::Commit));
  EXPECT_FALSE(TransactionPhaseManager::CanProceedToEndAtPhase(
      TransactionPhase::CommitNotify));
  EXPECT_TRUE(TransactionPhaseManager::CanProceedToEndAtPhase(
      TransactionPhase::Committed));
  EXPECT_FALSE(TransactionPhaseManager::CanProceedToEndAtPhase(
      TransactionPhase::AbortNotify));
  EXPECT_TRUE(TransactionPhaseManager::CanProceedToEndAtPhase(
      TransactionPhase::Aborted));
  EXPECT_TRUE(
      TransactionPhaseManager::CanProceedToEndAtPhase(TransactionPhase::End));
}

}  // namespace google::scp::core::test
