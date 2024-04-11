#ifndef CC_PBS_TOOLS_PBS_LOG_RECOVERY_READ_JOURNAL_INTERNAL_H_
#define CC_PBS_TOOLS_PBS_LOG_RECOVERY_READ_JOURNAL_INTERNAL_H_


#include "absl/status/statusor.h"
#include "absl/synchronization/blocking_counter.h"
#include "cc/core/blob_storage_provider/mock/mock_blob_storage_provider.h"

namespace google::scp::pbs {

// Reads a journal file from journal_file_path in the local file system.
absl::StatusOr<core::BytesBuffer> ReadJournalFile(
    core::blob_storage_provider::mock::MockBlobStorageClient& storage_client,
    absl::string_view journal_file_path,
    absl::BlockingCounter blocker = absl::BlockingCounter(1));

}  // namespace google::scp::pbs

#endif  // CC_PBS_TOOLS_PBS_LOG_RECOVERY_READ_JOURNAL_INTERNAL_H_
