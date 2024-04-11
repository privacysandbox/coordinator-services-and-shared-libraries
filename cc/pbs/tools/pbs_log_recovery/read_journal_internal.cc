#include "cc/pbs/tools/pbs_log_recovery/read_journal_internal.h"

#include <memory>
#include <string>

#include "absl/flags/internal/path_util.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/blocking_counter.h"
#include "cc/core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "cc/core/journal_service/interface/journal_service_stream_interface.h"
#include "cc/core/journal_service/src/proto/journal_service.pb.h"
#include "core/interface/type_def.h"

namespace google::scp::pbs {

using ::google::scp::core::AsyncContext;
using ::google::scp::core::BytesBuffer;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::GetBlobRequest;
using ::google::scp::core::GetBlobResponse;
using ::google::scp::core::blob_storage_provider::mock::MockBlobStorageClient;
using ::google::scp::core::errors::GetErrorMessage;

absl::StatusOr<BytesBuffer> ReadJournalFile(
    MockBlobStorageClient& storage_client, absl::string_view journal_file_path,
    absl::BlockingCounter blocker) {
  auto get_blob_request = std::make_shared<GetBlobRequest>();
  get_blob_request->bucket_name = std::make_shared<std::string>(
      absl::flags_internal::Package(journal_file_path));
  get_blob_request->blob_name = std::make_shared<std::string>(
      absl::flags_internal::Basename(journal_file_path));

  BytesBuffer journal_bytes_buffer;
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
      get_blob_request,
      [&journal_bytes_buffer,
       &blocker](AsyncContext<GetBlobRequest, GetBlobResponse>& context) {
        journal_bytes_buffer = *context.response->buffer;
        blocker.DecrementCount();
      });

  ExecutionResult result = storage_client.GetBlob(get_blob_context);
  if (!result.Successful()) {
    return absl::InternalError(GetErrorMessage(result.status_code));
  }

  blocker.Wait();

  return journal_bytes_buffer;
}

}  // namespace google::scp::pbs
