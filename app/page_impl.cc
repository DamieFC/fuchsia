// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/ledger/app/page_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "apps/ledger/app/page_snapshot_impl.h"
#include "apps/ledger/convert/convert.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/ftl/logging.h"
#include "lib/mtl/data_pipe/strings.h"
#include "lib/mtl/shared_buffer/strings.h"

namespace ledger {
namespace {
// Maximal size of data that will be returned inline.
const size_t kMaxInlineDataSize = 2048;

Status ConvertStatus(storage::Status status,
                     Status not_found_status = Status::INTERNAL_ERROR) {
  switch (status) {
    case storage::Status::OK:
      return Status::OK;
    case storage::Status::IO_ERROR:
      return Status::IO_ERROR;
    case storage::Status::NOT_FOUND:
      return not_found_status;
    default:
      return Status::INTERNAL_ERROR;
  }
}

Status ToBuffer(convert::ExtendedStringView value,
                int64_t offset,
                int64_t max_size,
                mojo::ScopedSharedBufferHandle* buffer) {
  size_t start = value.size();
  if (static_cast<size_t>(std::abs(offset)) < value.size()) {
    start = offset < 0 ? value.size() + offset : offset;
  }
  size_t length = max_size < 0 ? value.size() : max_size;

  bool result =
      mtl::SharedBufferFromString(value.substr(start, length), buffer);
  return result ? Status::OK : Status::UNKNOWN_ERROR;
}

}  // namespace

PageImpl::PageImpl(storage::PageStorage* storage) : storage_(storage) {}

PageImpl::~PageImpl() {}

storage::CommitId PageImpl::GetLocalBranchHeadCommit() {
  std::vector<storage::CommitId> commit_ids;
  // TODO(etiennej): Fail more nicely.
  FTL_CHECK(storage_->GetHeadCommitIds(&commit_ids) == storage::Status::OK);
  FTL_DCHECK(commit_ids.size() > 0);
  if (commit_ids.size() == 1) {
    return commit_ids[0];
  }
  // TODO(etiennej): Make sure we stay on the same branch. We can do it
  // inefficiently here, or maybe storage can keep some additional data for us?
  return commit_ids[0];
}

// GetId() => (array<uint8> id);
void PageImpl::GetId(const GetIdCallback& callback) {
  callback.Run(convert::ToArray(storage_->GetId()));
}

// GetSnapshot() => (Status status, PageSnapshot? snapshot);
void PageImpl::GetSnapshot(const GetSnapshotCallback& callback) {
  // TODO(etiennej): Commit implicit transactions when we have those.
  storage::CommitId commit_id;
  if (!journal_) {
    commit_id = GetLocalBranchHeadCommit();
  } else {
    commit_id = journal_parent_commit_;
  }
  std::unique_ptr<storage::Commit> commit;
  storage::Status status = storage_->GetCommit(commit_id, &commit);
  if (status != storage::Status::OK) {
    callback.Run(ConvertStatus(status), nullptr);
    return;
  }
  PageSnapshotPtr snapshot_ptr;
  page_snapshot_bindings_.AddBinding(
      new PageSnapshotImpl(storage_, commit->GetContents()),
      GetProxy(&snapshot_ptr));
  callback.Run(Status::OK, snapshot_ptr.Pass());
}

// Watch(PageWatcher watcher) => (Status status);
void PageImpl::Watch(mojo::InterfaceHandle<PageWatcher> watcher,
                     const WatchCallback& callback) {
  FTL_LOG(ERROR) << "PageImpl::Watch not implemented";
  callback.Run(Status::UNKNOWN_ERROR);
}

Status PageImpl::RunInTransaction(
    std::function<Status(storage::Journal* journal)> callback) {
  if (journal_) {
    // A transaction is in progress; add this change to it.
    return callback(journal_.get());
  }
  // No transaction is in progress; create one just for this change.
  // TODO(etiennej): Add a change batching strategy for operations outside
  // transactions. Currently, we create a commit for every change; we would
  // like to group changes that happen "close enough" together in one commit.
  storage::CommitId commit_id = GetLocalBranchHeadCommit();
  std::unique_ptr<storage::Journal> journal;
  storage::Status status = storage_->StartCommit(
      commit_id, storage::JournalType::IMPLICIT, &journal);
  if (status != storage::Status::OK) {
    return ConvertStatus(status);
  }
  Status ledger_status = callback(journal.get());
  if (ledger_status != Status::OK) {
    return ledger_status;
  }
  storage::CommitId new_commit_id;
  status = journal->Commit(&new_commit_id);
  return ConvertStatus(status);
}

// Put(array<uint8> key, array<uint8> value) => (Status status);
void PageImpl::Put(mojo::Array<uint8_t> key,
                   mojo::Array<uint8_t> value,
                   const PutCallback& callback) {
  PutWithPriority(std::move(key), std::move(value), Priority::EAGER, callback);
}

// PutWithPriority(array<uint8> key, array<uint8> value, Priority priority)
//   => (Status status);
void PageImpl::PutWithPriority(mojo::Array<uint8_t> key,
                               mojo::Array<uint8_t> value,
                               Priority priority,
                               const PutWithPriorityCallback& callback) {
  // TODO(etiennej): Use asynchronous write, otherwise the run loop may block
  // until the pipe is drained.
  mojo::ScopedDataPipeConsumerHandle data_pipe =
      mtl::WriteStringToConsumerHandle(convert::ToStringView(value));
  storage_->AddObjectFromLocal(
      std::move(data_pipe), value.size(),
      ftl::MakeCopyable([ this, key = std::move(key), priority, callback ](
          storage::Status status, storage::ObjectId object_id) {
        if (status != storage::Status::OK) {
          callback.Run(ConvertStatus(status));
          return;
        }

        callback.Run(
            PutInCommit(key, object_id, priority == Priority::EAGER
                                            ? storage::KeyPriority::EAGER
                                            : storage::KeyPriority::LAZY));
      }));
}

// PutReference(array<uint8> key, Reference? reference, Priority priority)
//   => (Status status);
void PageImpl::PutReference(mojo::Array<uint8_t> key,
                            ReferencePtr reference,
                            Priority priority,
                            const PutReferenceCallback& callback) {
  storage::ObjectIdView object_id(reference->opaque_id);
  callback.Run(PutInCommit(key, object_id, priority == Priority::EAGER
                                               ? storage::KeyPriority::EAGER
                                               : storage::KeyPriority::LAZY));
}

Status PageImpl::PutInCommit(convert::ExtendedStringView key,
                             storage::ObjectIdView object_id,
                             storage::KeyPriority priority) {
  return RunInTransaction(
      [&key, &object_id, &priority](storage::Journal* journal) {
        return ConvertStatus(journal->Put(key, object_id, priority));
      });
}

// Delete(array<uint8> key) => (Status status);
void PageImpl::Delete(mojo::Array<uint8_t> key,
                      const DeleteCallback& callback) {
  callback.Run(RunInTransaction([&key](storage::Journal* journal) {
    return ConvertStatus(journal->Delete(key), Status::KEY_NOT_FOUND);
  }));
}

// CreateReference(int64 size, handle<data_pipe_producer> data)
//   => (Status status, Reference reference);
void PageImpl::CreateReference(int64_t size,
                               mojo::ScopedDataPipeConsumerHandle data,
                               const CreateReferenceCallback& callback) {
  storage_->AddObjectFromLocal(
      std::move(data), size,
      [callback](storage::Status status, storage::ObjectId object_id) {
        if (status != storage::Status::OK) {
          callback.Run(ConvertStatus(status), nullptr);
          return;
        }

        ReferencePtr reference = Reference::New();
        reference->opaque_id = convert::ToArray(object_id);
        callback.Run(Status::OK, std::move(reference));
      });
}

// GetReference(Reference reference) => (Status status, Value? value);
void PageImpl::GetReference(ReferencePtr reference,
                            const GetReferenceCallback& callback) {
  GetReferenceInternal(std::move(reference),
                       [this, &callback](Status status, ftl::StringView data) {
                         if (status != Status::OK) {
                           callback.Run(status, nullptr);
                           return;
                         }
                         if (data.size() <= kMaxInlineDataSize) {
                           ValuePtr value = Value::New();
                           value->set_bytes(convert::ToArray(data));
                           callback.Run(Status::OK, std::move(value));
                           return;
                         }

                         mojo::ScopedSharedBufferHandle buffer;
                         Status mojo_status = ToBuffer(data, 0, -1, &buffer);
                         if (mojo_status != Status::OK) {
                           callback.Run(mojo_status, nullptr);
                           return;
                         }
                         ValuePtr value = Value::New();
                         value->set_buffer(std::move(buffer));
                         callback.Run(Status::OK, std::move(value));
                       });
}

// GetPartialReference(Reference reference, int64 offset, int64 max_size)
//   => (Status status, handle<shared_buffer>? buffer);
void PageImpl::GetPartialReference(
    ReferencePtr reference,
    int64_t offset,
    int64_t max_size,
    const GetPartialReferenceCallback& callback) {
  GetReferenceInternal(
      std::move(reference),
      [this, offset, max_size, &callback](Status status, ftl::StringView data) {
        if (status != Status::OK) {
          callback.Run(status, mojo::ScopedSharedBufferHandle());
          return;
        }
        mojo::ScopedSharedBufferHandle buffer;
        Status mojo_status = ToBuffer(data, offset, max_size, &buffer);
        if (mojo_status != Status::OK) {
          callback.Run(mojo_status, mojo::ScopedSharedBufferHandle());
          return;
        }
        callback.Run(Status::OK, std::move(buffer));
      });
}

// StartTransaction() => (Status status);
void PageImpl::StartTransaction(const StartTransactionCallback& callback) {
  if (journal_) {
    callback.Run(Status::TRANSACTION_ALREADY_IN_PROGRESS);
    return;
  }
  storage::CommitId commit_id = GetLocalBranchHeadCommit();
  storage::Status status = storage_->StartCommit(
      commit_id, storage::JournalType::EXPLICIT, &journal_);
  journal_parent_commit_ = commit_id;
  callback.Run(ConvertStatus(status));
}

// Commit() => (Status status);
void PageImpl::Commit(const CommitCallback& callback) {
  if (!journal_) {
    callback.Run(Status::NO_TRANSACTION_IN_PROGRESS);
    return;
  }
  storage::CommitId new_commit_id;
  storage::Status status = journal_->Commit(&new_commit_id);
  journal_.reset();
  journal_parent_commit_.clear();
  callback.Run(ConvertStatus(status));
}

// Rollback() => (Status status);
void PageImpl::Rollback(const RollbackCallback& callback) {
  if (!journal_) {
    callback.Run(Status::NO_TRANSACTION_IN_PROGRESS);
    return;
  }
  storage::Status status = journal_->Rollback();
  journal_.reset();
  journal_parent_commit_.clear();
  callback.Run(ConvertStatus(status));
}

void PageImpl::GetReferenceInternal(
    ReferencePtr reference,
    std::function<void(Status, ftl::StringView)> callback) {
  storage_->GetObject(
      reference->opaque_id,
      [callback](storage::Status status,
                 std::unique_ptr<const storage::Object> object) {
        if (status != storage::Status::OK) {
          callback(ConvertStatus(status, Status::REFERENCE_NOT_FOUND),
                   ftl::StringView());
          return;
        }
        ftl::StringView data;
        status = object->GetData(&data);
        if (status != storage::Status::OK) {
          callback(ConvertStatus(status, Status::REFERENCE_NOT_FOUND),
                   ftl::StringView());
          return;
        }

        callback(Status::OK, data);
      });
}

}  // namespace ledger
