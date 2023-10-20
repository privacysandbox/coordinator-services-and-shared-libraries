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

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "public/cpio/interface/cpio.h"

using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::ConcurrentQueue;
using google::scp::core::common::OperationDispatcher;
using google::scp::core::common::RetryStrategy;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::BlobStorageClientFactory;
using google::scp::cpio::BlobStorageClientInterface;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::client_providers::GlobalCpio;
using std::atomic_bool;
using std::atomic_int;
using std::cerr;
using std::cout;
using std::endl;
using std::function;
using std::make_shared;
using std::move;
using std::mutex;
using std::scoped_lock;
using std::shared_ptr;
using std::unique_ptr;

namespace {
constexpr char kBucketName[] = "blob-storage-service-test-bucket";
constexpr char kBlobName[] = "some_blob_name";
}  // namespace

int main(int argc, char* argv[]) {
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    cerr << "Failed to initialize CPIO: " << GetErrorMessage(result.status_code)
         << endl;
  }

  auto blob_storage_client = BlobStorageClientFactory::Create();
  result = blob_storage_client->Init();
  if (!result.Successful()) {
    cerr << "Failed to Init BlobStorageClient: "
         << GetErrorMessage(result.status_code) << endl;
  }
  result = blob_storage_client->Run();
  if (!result.Successful()) {
    cerr << "Failed to Run BlobStorageClient: "
         << GetErrorMessage(result.status_code) << endl;
  }

  {
    // PutBlob.
    auto data = "some data string";
    atomic_bool finished(false);
    ExecutionResult result;
    auto put_blob_request = make_shared<PutBlobRequest>();
    put_blob_request->mutable_blob()->mutable_metadata()->set_bucket_name(
        kBucketName);
    put_blob_request->mutable_blob()->mutable_metadata()->set_blob_name(
        kBlobName);
    put_blob_request->mutable_blob()->set_data(data);
    AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context(
        move(put_blob_request), [&result, &finished](auto& context) {
          result = context.result;
          // No other contents in PutBlobResponse.
          finished = true;
        });
    auto put_blob_result = blob_storage_client->PutBlob(put_blob_context);
    if (!put_blob_result.Successful()) {
      cerr << "Putting blob failed: "
           << GetErrorMessage(put_blob_result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
    WaitUntil([&finished]() { return finished.load(); });
    if (!result.Successful()) {
      cerr << "Putting blob failed asynchronously: "
           << GetErrorMessage(result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
  }
  {
    // GetBlob.
    atomic_bool finished(false);
    ExecutionResult result;
    auto get_blob_request = make_shared<GetBlobRequest>();
    get_blob_request->mutable_blob_metadata()->set_bucket_name(kBucketName);
    get_blob_request->mutable_blob_metadata()->set_blob_name(kBlobName);
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
        move(get_blob_request), [&result, &finished](auto& context) {
          result = context.result;
          if (result.Successful()) {
            cout << "Got blob: " << context.response->DebugString();
          }
          finished = true;
        });
    auto get_blob_result = blob_storage_client->GetBlob(get_blob_context);
    if (!get_blob_result.Successful()) {
      cerr << "Getting blob failed: "
           << GetErrorMessage(get_blob_result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
    WaitUntil([&finished]() { return finished.load(); });
    if (!result.Successful()) {
      cerr << "Getting blob failed asynchronously: "
           << GetErrorMessage(result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
  }
  {
    // ListBlobsMetadata.
    atomic_bool finished(false);
    ExecutionResult result;
    auto list_blobs_metadata_request = make_shared<ListBlobsMetadataRequest>();
    list_blobs_metadata_request->mutable_blob_metadata()->set_bucket_name(
        kBucketName);
    AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>
        list_blobs_metadata_context(move(list_blobs_metadata_request),
                                    [&result, &finished](auto& context) {
                                      result = context.result;
                                      if (result.Successful()) {
                                        cout << "Listed blobs: "
                                             << context.response->DebugString();
                                      }
                                      finished = true;
                                    });
    auto list_blobs_metadata_result =
        blob_storage_client->ListBlobsMetadata(list_blobs_metadata_context);
    if (!list_blobs_metadata_result.Successful()) {
      cerr << "Listing blobs failed: "
           << GetErrorMessage(list_blobs_metadata_result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
    WaitUntil([&finished]() { return finished.load(); });
    if (!result.Successful()) {
      cerr << "Listing blobs failed asynchronously: "
           << GetErrorMessage(result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
  }
  {
    // DeleteBlob.
    atomic_bool finished(false);
    ExecutionResult result;
    auto delete_blob_request = make_shared<DeleteBlobRequest>();
    delete_blob_request->mutable_blob_metadata()->set_bucket_name(kBucketName);
    delete_blob_request->mutable_blob_metadata()->set_blob_name(kBlobName);
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context(
        move(delete_blob_request), [&result, &finished](auto& context) {
          result = context.result;
          // No other contents in DeleteBlobResponse.
          finished = true;
        });
    auto delete_blob_result =
        blob_storage_client->DeleteBlob(delete_blob_context);
    if (!delete_blob_result.Successful()) {
      cerr << "Deleting blob failed: "
           << GetErrorMessage(delete_blob_result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
    WaitUntil([&finished]() { return finished.load(); });
    if (!result.Successful()) {
      cerr << "Deleting blob failed asynchronously: "
           << GetErrorMessage(result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
  }

#ifdef GCP_CPIO

  {
    // PutBlobStream.

    atomic_bool finished(false);
    auto put_blob_stream_request = make_shared<PutBlobStreamRequest>();
    put_blob_stream_request->mutable_blob_portion()
        ->mutable_metadata()
        ->set_bucket_name(kBucketName);
    put_blob_stream_request->mutable_blob_portion()
        ->mutable_metadata()
        ->set_blob_name(kBlobName);
    put_blob_stream_request->mutable_blob_portion()->set_data("some");

    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
        put_blob_stream_context;
    put_blob_stream_context.request = move(put_blob_stream_request);
    put_blob_stream_context.callback = [&result, &finished](auto& context) {
      result = context.result;
      // No other contents in PutBlobStreamResponse.
      finished = true;
    };

    auto put_blob_stream_result =
        blob_storage_client->PutBlobStream(put_blob_stream_context);
    if (!put_blob_stream_result.Successful()) {
      cerr << "Putting blob failed: "
           << GetErrorMessage(put_blob_stream_result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
    // After this point, the client is waiting for elements to be pushed
    // onto the queue.

    PutBlobStreamRequest request;
    request.mutable_blob_portion()->mutable_metadata()->set_bucket_name(
        kBucketName);
    request.mutable_blob_portion()->mutable_metadata()->set_blob_name(
        kBlobName);
    request.mutable_blob_portion()->set_data(" other");
    // Note, generally one should `move` elements onto the context but we
    // don't here which incurs a copy.
    if (auto result = put_blob_stream_context.TryPushRequest(request);
        !result.Successful()) {
      cerr << "Failed enqueueing a new element" << endl;
      exit(EXIT_FAILURE);
    }

    request.mutable_blob_portion()->set_data(" data");
    if (auto result = put_blob_stream_context.TryPushRequest(request);
        !result.Successful()) {
      cerr << "Failed enqueueing a new element" << endl;
      exit(EXIT_FAILURE);
    }

    // Marking the context done here tells the client to finalize the upload
    // and will call the context's callback.
    put_blob_stream_context.MarkDone();

    WaitUntil([&finished]() { return finished.load(); });
    if (!result.Successful()) {
      cerr << "Putting blob failed asynchronously: "
           << GetErrorMessage(result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
  }
  {
    // GetBlobStream - callback version.
    mutex log_mutex;
    atomic_bool finished(false);
    auto get_blob_stream_request = make_shared<GetBlobStreamRequest>();
    get_blob_stream_request->mutable_blob_metadata()->set_bucket_name(
        kBucketName);
    get_blob_stream_request->mutable_blob_metadata()->set_blob_name(kBlobName);
    get_blob_stream_request->set_max_bytes_per_response(5);
    ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
        get_blob_stream_context;
    get_blob_stream_context.request = move(get_blob_stream_request);

    get_blob_stream_context.process_callback =
        [&result, &finished, &log_mutex](auto& context, bool is_finish) {
          if (is_finish) {
            result = context.result;
          }
          auto resp = context.TryGetNextResponse();
          if (resp == nullptr) {
            // If dequeueing is unsuccessful, then context should be done.
            if (!context.IsMarkedDone()) {
              scoped_lock lock(log_mutex);
              cerr << "This should never happen" << endl;
            }
            finished = true;
          } else {
            scoped_lock lock(log_mutex);
            cout << "Got blob portion: " << resp->DebugString();
          }
        };

    blob_storage_client->GetBlobStream(get_blob_stream_context);

    WaitUntil([&finished]() { return finished.load(); });
    if (!result.Successful()) {
      cerr << "Getting blob stream failed asynchronously: "
           << GetErrorMessage(result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
  }
  {
    // GetBlobStream - polling version.
    atomic_bool finished(false);
    auto get_blob_stream_request = make_shared<GetBlobStreamRequest>();
    get_blob_stream_request->mutable_blob_metadata()->set_bucket_name(
        kBucketName);
    get_blob_stream_request->mutable_blob_metadata()->set_blob_name(kBlobName);
    get_blob_stream_request->set_max_bytes_per_response(5);
    ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
        get_blob_stream_context;
    get_blob_stream_context.request = move(get_blob_stream_request);

    get_blob_stream_context.process_callback =
        [&result, &finished](auto& context, bool is_finish) {
          if (is_finish) {
            result = context.result;
            finished = true;
          }
        };

    auto get_blob_stream_result =
        blob_storage_client->GetBlobStream(get_blob_stream_context);
    if (!get_blob_stream_result.Successful()) {
      cerr << "Getting blob stream failed: "
           << GetErrorMessage(get_blob_stream_result.status_code) << endl;
      exit(EXIT_FAILURE);
    }

    unique_ptr<GetBlobStreamResponse> resp = nullptr;
    auto context_is_done = [&get_blob_stream_context, &resp]() -> bool {
      resp = get_blob_stream_context.TryGetNextResponse();
      if (resp == nullptr && get_blob_stream_context.IsMarkedDone()) {
        // It's possible resp can be nullptr but a response is pushed and the
        // queue marked done before we check it. Catch that edge case here.
        resp = get_blob_stream_context.TryGetNextResponse();
        return resp == nullptr;
      }
      return false;
    };
    while (!context_is_done()) {
      if (resp == nullptr) {
        continue;
      }
      cout << "Got blob portion: " << resp->DebugString() << endl;
    }

    WaitUntil([&finished]() { return finished.load(); });
    if (!result.Successful()) {
      cerr << "Getting blob stream failed asynchronously: "
           << GetErrorMessage(result.status_code) << endl;
      exit(EXIT_FAILURE);
    }
  }
#endif

  cout << "Done :)" << endl;
  return EXIT_SUCCESS;
}
