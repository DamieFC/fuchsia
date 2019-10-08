// WARNING: This file is machine generated by fidlgen.

#pragma once

#include <lib/fidl/internal.h>
#include <lib/fidl/llcpp/array.h>
#include <lib/fidl/llcpp/coding.h>
#include <lib/fidl/llcpp/string_view.h>
#include <lib/fidl/llcpp/sync_call.h>
#include <lib/fidl/llcpp/traits.h>
#include <lib/fidl/llcpp/transaction.h>
#include <lib/fidl/llcpp/vector_view.h>
#include <lib/fit/function.h>
#include <lib/zx/channel.h>
#include <zircon/fidl.h>

#include <fuchsia/io/llcpp/fidl.h>

namespace llcpp {

namespace fuchsia {
namespace tee {
namespace manager {

class Provider;

extern "C" const fidl_type_t fuchsia_tee_manager_ProviderRequestPersistentStorageRequestTable;

// Provider provides service access and support to the TEE driver
// for things like persistent storage, since the TEE may make upward RPC-like
// requests to the REE.
class Provider final {
  Provider() = delete;
 public:

  struct RequestPersistentStorageRequest final {
    FIDL_ALIGNDECL
    fidl_message_header_t _hdr;
    ::zx::channel dir;

    static constexpr const fidl_type_t* Type = &fuchsia_tee_manager_ProviderRequestPersistentStorageRequestTable;
    static constexpr uint32_t MaxNumHandles = 1;
    static constexpr uint32_t PrimarySize = 24;
    static constexpr uint32_t MaxOutOfLine = 0;
    static constexpr bool HasFlexibleEnvelope = false;
    static constexpr ::fidl::internal::TransactionalMessageKind MessageKind =
        ::fidl::internal::TransactionalMessageKind::kRequest;
  };


  // Collection of return types of FIDL calls in this interface.
  class ResultOf final {
    ResultOf() = delete;
   private:
    class RequestPersistentStorage_Impl final : private ::fidl::internal::StatusAndError {
      using Super = ::fidl::internal::StatusAndError;
     public:
      RequestPersistentStorage_Impl(zx::unowned_channel _client_end, ::zx::channel dir);
      ~RequestPersistentStorage_Impl() = default;
      RequestPersistentStorage_Impl(RequestPersistentStorage_Impl&& other) = default;
      RequestPersistentStorage_Impl& operator=(RequestPersistentStorage_Impl&& other) = default;
      using Super::status;
      using Super::error;
      using Super::ok;
    };

   public:
    using RequestPersistentStorage = RequestPersistentStorage_Impl;
  };

  // Collection of return types of FIDL calls in this interface,
  // when the caller-allocate flavor or in-place call is used.
  class UnownedResultOf final {
    UnownedResultOf() = delete;
   private:
    class RequestPersistentStorage_Impl final : private ::fidl::internal::StatusAndError {
      using Super = ::fidl::internal::StatusAndError;
     public:
      RequestPersistentStorage_Impl(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::zx::channel dir);
      ~RequestPersistentStorage_Impl() = default;
      RequestPersistentStorage_Impl(RequestPersistentStorage_Impl&& other) = default;
      RequestPersistentStorage_Impl& operator=(RequestPersistentStorage_Impl&& other) = default;
      using Super::status;
      using Super::error;
      using Super::ok;
    };

   public:
    using RequestPersistentStorage = RequestPersistentStorage_Impl;
  };

  class SyncClient final {
   public:
    explicit SyncClient(::zx::channel channel) : channel_(std::move(channel)) {}
    ~SyncClient() = default;
    SyncClient(SyncClient&&) = default;
    SyncClient& operator=(SyncClient&&) = default;

    const ::zx::channel& channel() const { return channel_; }

    ::zx::channel* mutable_channel() { return &channel_; }

    // Allocates 24 bytes of message buffer on the stack. No heap allocation necessary.
    ResultOf::RequestPersistentStorage RequestPersistentStorage(::zx::channel dir);

    // Caller provides the backing storage for FIDL message via request and response buffers.
    UnownedResultOf::RequestPersistentStorage RequestPersistentStorage(::fidl::BytePart _request_buffer, ::zx::channel dir);

   private:
    ::zx::channel channel_;
  };

  // Methods to make a sync FIDL call directly on an unowned channel, avoiding setting up a client.
  class Call final {
    Call() = delete;
   public:

    // Allocates 24 bytes of message buffer on the stack. No heap allocation necessary.
    static ResultOf::RequestPersistentStorage RequestPersistentStorage(zx::unowned_channel _client_end, ::zx::channel dir);

    // Caller provides the backing storage for FIDL message via request and response buffers.
    static UnownedResultOf::RequestPersistentStorage RequestPersistentStorage(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::zx::channel dir);

  };

  // Messages are encoded and decoded in-place when these methods are used.
  // Additionally, requests must be already laid-out according to the FIDL wire-format.
  class InPlace final {
    InPlace() = delete;
   public:

    static ::fidl::internal::StatusAndError RequestPersistentStorage(zx::unowned_channel _client_end, ::fidl::DecodedMessage<RequestPersistentStorageRequest> params);

  };

  // Pure-virtual interface to be implemented by a server.
  class Interface {
   public:
    Interface() = default;
    virtual ~Interface() = default;
    using _Outer = Provider;
    using _Base = ::fidl::CompleterBase;

    using RequestPersistentStorageCompleter = ::fidl::Completer<>;

    virtual void RequestPersistentStorage(::zx::channel dir, RequestPersistentStorageCompleter::Sync _completer) = 0;

  };

  // Attempts to dispatch the incoming message to a handler function in the server implementation.
  // If there is no matching handler, it returns false, leaving the message and transaction intact.
  // In all other cases, it consumes the message and returns true.
  // It is possible to chain multiple TryDispatch functions in this manner.
  static bool TryDispatch(Interface* impl, fidl_msg_t* msg, ::fidl::Transaction* txn);

  // Dispatches the incoming message to one of the handlers functions in the interface.
  // If there is no matching handler, it closes all the handles in |msg| and closes the channel with
  // a |ZX_ERR_NOT_SUPPORTED| epitaph, before returning false. The message should then be discarded.
  static bool Dispatch(Interface* impl, fidl_msg_t* msg, ::fidl::Transaction* txn);

  // Same as |Dispatch|, but takes a |void*| instead of |Interface*|. Only used with |fidl::Bind|
  // to reduce template expansion.
  // Do not call this method manually. Use |Dispatch| instead.
  static bool TypeErasedDispatch(void* impl, fidl_msg_t* msg, ::fidl::Transaction* txn) {
    return Dispatch(static_cast<Interface*>(impl), msg, txn);
  }


  // Helper functions to fill in the transaction header in a |DecodedMessage<TransactionalMessage>|.
  class SetTransactionHeaderFor final {
    SetTransactionHeaderFor() = delete;
   public:
    static void RequestPersistentStorageRequest(const ::fidl::DecodedMessage<Provider::RequestPersistentStorageRequest>& _msg);
  };
};

}  // namespace manager
}  // namespace tee
}  // namespace fuchsia
}  // namespace llcpp

namespace fidl {

template <>
struct IsFidlType<::llcpp::fuchsia::tee::manager::Provider::RequestPersistentStorageRequest> : public std::true_type {};
template <>
struct IsFidlMessage<::llcpp::fuchsia::tee::manager::Provider::RequestPersistentStorageRequest> : public std::true_type {};
static_assert(sizeof(::llcpp::fuchsia::tee::manager::Provider::RequestPersistentStorageRequest)
    == ::llcpp::fuchsia::tee::manager::Provider::RequestPersistentStorageRequest::PrimarySize);
static_assert(offsetof(::llcpp::fuchsia::tee::manager::Provider::RequestPersistentStorageRequest, dir) == 16);

}  // namespace fidl
