#include "cc/pbs/tools/pbs_log_recovery/read_journal_internal.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/blocking_counter.h"
#include "cc/core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "cc/public/core/interface/execution_result.h"

namespace google::scp::pbs {
namespace {

using ::google::scp::core::AsyncContext;
using ::google::scp::core::BytesBuffer;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::GetBlobRequest;
using ::google::scp::core::GetBlobResponse;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::blob_storage_provider::mock::MockBlobStorageClient;
using ::testing::_;
using ::testing::NotNull;
using ::testing::Return;

class TestableMockBlobStorageClient : public MockBlobStorageClient {
 public:
  MOCK_METHOD(ExecutionResult, GetBlob,
              ((AsyncContext<GetBlobRequest, GetBlobResponse> &
                get_blob_context)),
              (noexcept));
};

MATCHER_P2(GetBlobRequestHasRightBlobAndBucketName, bucket_name, blob_name,
           "") {
  const AsyncContext<GetBlobRequest, GetBlobResponse>& input = arg;
  if (*input.request->blob_name != blob_name ||
      *input.request->bucket_name != bucket_name) {
    return false;
  }
  return true;
}

TEST(ReadJournalInternalTest, ReadJournalFileTest) {
  TestableMockBlobStorageClient storage_client;
  static constexpr char kBucketName[] = "cc/pbs/tools/pbs_log_recovery/";
  static constexpr char kBlobName[] =
      "00000000-0000-0000-0000-000000000000_journal_01704737324757218424";
  EXPECT_CALL(storage_client, GetBlob(GetBlobRequestHasRightBlobAndBucketName(
                                  kBucketName, kBlobName)))
      .WillOnce(Return(SuccessExecutionResult()));
  absl::StatusOr<const BytesBuffer> journal_bytes_buffer =
      ReadJournalFile(storage_client, absl::StrCat(kBucketName, kBlobName),
                      absl::BlockingCounter(0));
  ASSERT_TRUE(journal_bytes_buffer.ok());
}

}  // namespace
}  // namespace google::scp::pbs
