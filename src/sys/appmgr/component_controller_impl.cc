// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sys/appmgr/component_controller_impl.h"

#include <lib/async/default.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/fdio.h>
#include <lib/fit/function.h>
#include <zircon/types.h>

#include <cinttypes>
#include <utility>

#include <fbl/string_printf.h>
#include <fs/pseudo-file.h>
#include <fs/remote-dir.h>
#include <fs/service.h>
#include <trace/event.h>

#include "src/lib/fsl/handles/object_info.h"
#include "src/lib/fxl/logging.h"
#include "src/sys/appmgr/component_container.h"
#include "src/sys/appmgr/namespace.h"

namespace component {

using fuchsia::sys::TerminationReason;

namespace {
zx::process DuplicateProcess(const zx::process& process) {
  zx::process ret;
  zx_status_t status = process.duplicate(ZX_RIGHT_SAME_RIGHTS, &ret);
  if (status != ZX_OK) {
    FXL_LOG(ERROR) << "Failed to duplicate process handle: " << status;
  }
  return ret;
}
}  // namespace

ComponentRequestWrapper::ComponentRequestWrapper(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController> request, int64_t default_return,
    fuchsia::sys::TerminationReason default_reason)
    : request_(std::move(request)), return_code_(default_return), reason_(default_reason) {}

ComponentRequestWrapper::~ComponentRequestWrapper() {
  if (request_) {
    FailedComponentController failed(return_code_, reason_, std::move(request_));
  }
}

ComponentRequestWrapper::ComponentRequestWrapper(ComponentRequestWrapper&& other) {
  *this = std::move(other);
}

void ComponentRequestWrapper::operator=(ComponentRequestWrapper&& other) {
  request_ = std::move(other.request_);
  return_code_ = std::move(other.return_code_);
  reason_ = std::move(other.reason_);
}

void ComponentRequestWrapper::SetReturnValues(int64_t return_code, TerminationReason reason) {
  return_code_ = return_code;
  reason_ = reason;
}

FailedComponentController::FailedComponentController(
    int64_t return_code, TerminationReason termination_reason,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController> controller)
    : binding_(this), return_code_(return_code), termination_reason_(termination_reason) {
  if (controller) {
    binding_.Bind(std::move(controller));
  }
}

FailedComponentController::~FailedComponentController() {
  // This can be false if other side of the channel dies before this object dies.
  if (binding_.is_bound()) {
    binding_.events().OnTerminated(return_code_, termination_reason_);
  }
}

void FailedComponentController::Kill() {}

void FailedComponentController::Detach() {}

ComponentControllerBase::ComponentControllerBase(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController> request, std::string url,
    std::string args, std::string label, std::string hub_instance_id, fxl::RefPtr<Namespace> ns,
    zx::channel exported_dir, zx::channel client_request)
    : binding_(this),
      label_(std::move(label)),
      hub_instance_id_(std::move(hub_instance_id)),
      hub_(fbl::AdoptRef(new fs::PseudoDir())),
      ns_(std::move(ns)) {
  if (request.is_valid()) {
    binding_.Bind(std::move(request));
    binding_.set_error_handler([this](zx_status_t /*status*/) { Kill(); });
  }
  if (!exported_dir) {
    return;
  }
  exported_dir_.Bind(std::move(exported_dir), async_get_default_dispatcher());

  if (client_request) {
    fdio_service_connect_at(exported_dir_.channel().get(), "svc", client_request.release());
  }

  hub_.SetName(label_);
  hub_.AddEntry("url", std::move(url));
  hub_.AddEntry("args", std::move(args));
  exported_dir_->Clone(fuchsia::io::OPEN_FLAG_DESCRIBE | fuchsia::io::OPEN_RIGHT_READABLE |
                           fuchsia::io::OPEN_RIGHT_WRITABLE,
                       cloned_exported_dir_.NewRequest());

  cloned_exported_dir_.events().OnOpen = [this](zx_status_t status,
                                                std::unique_ptr<fuchsia::io::NodeInfo> /*info*/) {
    if (status != ZX_OK) {
      FXL_LOG(WARNING) << "could not bind out directory for component" << label_ << "): " << status;
      return;
    }
    auto output_dir = fbl::AdoptRef(new fs::RemoteDir(cloned_exported_dir_.Unbind().TakeChannel()));
    hub_.PublishOut(std::move(output_dir));
    TRACE_DURATION_BEGIN("appmgr", "ComponentController::OnDirectoryReady");
    SendOnDirectoryReadyEvent();
    TRACE_DURATION_END("appmgr", "ComponentController::OnDirectoryReady");
  };

  cloned_exported_dir_.set_error_handler(
      [this](zx_status_t status) { cloned_exported_dir_.Unbind(); });
}

void ComponentControllerBase::SendOnDirectoryReadyEvent() {
  // This can be false if
  // 1. Other side of the channel dies before this call happens.
  // 2. Component Controller request was not passed while creating the component.
  if (binding_.is_bound()) {
    binding_.events().OnDirectoryReady();
  }
}

void ComponentControllerBase::SendOnTerminationEvent(
    int64_t return_code, fuchsia::sys::TerminationReason termination_reason) {
  // `binding_.is_bound()` can be false if
  //  1. Other side of the channel dies before this call happens.
  //  2. Component Controller request was not passed while creating the component.
  if (on_terminated_event_sent_ || !binding_.is_bound()) {
    return;
  }
  FXL_VLOG(1) << "Sending termination callback with return code: " << return_code;
  binding_.events().OnTerminated(return_code, termination_reason);
  on_terminated_event_sent_ = true;
}

ComponentControllerBase::~ComponentControllerBase() = default;

HubInfo ComponentControllerBase::HubInfo() {
  return component::HubInfo(label_, hub_instance_id_, hub_.dir());
}

void ComponentControllerBase::Detach() {
  binding_.set_error_handler([](zx_status_t /*status*/) {});
}

ComponentControllerImpl::ComponentControllerImpl(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController> request,
    ComponentContainer<ComponentControllerImpl>* container, zx::job job, zx::process process,
    std::string url, std::string args, std::string label, fxl::RefPtr<Namespace> ns,
    zx::channel exported_dir, zx::channel client_request)
    : ComponentControllerBase(std::move(request), std::move(url), std::move(args), std::move(label),
                              std::to_string(fsl::GetKoid(process.get())), std::move(ns),
                              std::move(exported_dir), std::move(client_request)),
      container_(container),
      job_(std::move(job)),
      process_(std::move(process)),
      koid_(std::to_string(fsl::GetKoid(process_.get()))),
      wait_(this, process_.get(), ZX_TASK_TERMINATED),
      system_objects_directory_(DuplicateProcess(process_)) {
  zx_status_t status = wait_.Begin(async_get_default_dispatcher());
  FXL_DCHECK(status == ZX_OK);

  hub()->SetJobId(std::to_string(fsl::GetKoid(job_.get())));
  hub()->SetProcessId(koid_);

  // Serve connections to the system_objects interface.
  auto system_objects = fbl::MakeRefCounted<fs::PseudoDir>();
  system_objects->AddEntry(
      fuchsia::inspect::deprecated::Inspect::Name_,
      fbl::MakeRefCounted<fs::Service>([this](zx::channel channel) {
        system_directory_bindings_.AddBinding(
            system_objects_directory_.object(),
            fidl::InterfaceRequest<fuchsia::inspect::deprecated::Inspect>(std::move(channel)),
            nullptr);
        return ZX_OK;
      }));

  hub()->AddEntry("system_objects", system_objects);

  hub()->AddIncomingServices(this->incoming_services());
}

ComponentControllerImpl::~ComponentControllerImpl() {
  // Two ways we end up here:
  // 1) OnHandleReady() destroys this object; in which case, process is dead.
  // 2) Our owner destroys this object; in which case, the process may still be
  //    alive.
  if (job_) {
    job_.kill();
    // Our owner destroyed this object before we could obtain a termination
    // reason.

    SendOnTerminationEvent(-1, TerminationReason::UNKNOWN);
  }
}

void ComponentControllerImpl::Kill() {
  FXL_VLOG(1) << "ComponentControllerImpl::Kill() called";
  TRACE_DURATION("appmgr", "ComponentController::Kill");
  if (job_) {
    job_.kill();
    job_.reset();
  }
}

bool ComponentControllerImpl::SendReturnCodeIfTerminated() {
  // Get process info.
  zx_info_process_t process_info;
  zx_status_t result =
      process_.get_info(ZX_INFO_PROCESS, &process_info, sizeof(process_info), NULL, NULL);
  FXL_DCHECK(result == ZX_OK);

  if (process_info.exited) {
    SendOnTerminationEvent(process_info.return_code, TerminationReason::EXITED);
  }

  return process_info.exited;
}

zx_status_t ComponentControllerImpl::AddSubComponentHub(const component::HubInfo& hub_info) {
  hub()->EnsureComponentDir();
  return hub()->AddComponent(hub_info);
}

zx_status_t ComponentControllerImpl::RemoveSubComponentHub(const component::HubInfo& hub_info) {
  return hub()->RemoveComponent(hub_info);
}

// Called when process terminates, regardless of if Kill() was invoked.
void ComponentControllerImpl::Handler(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                                      zx_status_t status, const zx_packet_signal* signal) {
  FXL_DCHECK(status == ZX_OK);
  FXL_DCHECK(signal->observed == ZX_TASK_TERMINATED);
  FXL_VLOG(1) << "ComponentControllerImpl::Handler() called";
  bool terminated = SendReturnCodeIfTerminated();
  FXL_DCHECK(terminated);

  process_.reset();

  container_->ExtractComponent(this);
  // The destructor of the temporary returned by ExtractComponent destroys
  // |this| at the end of the previous statement.
}

ComponentBridge::ComponentBridge(fidl::InterfaceRequest<fuchsia::sys::ComponentController> request,
                                 fuchsia::sys::ComponentControllerPtr remote_controller,
                                 ComponentContainer<ComponentBridge>* container, std::string url,
                                 std::string args, std::string label, std::string hub_instance_id,
                                 fxl::RefPtr<Namespace> ns, zx::channel exported_dir,
                                 zx::channel client_request)
    : ComponentControllerBase(std::move(request), std::move(url), std::move(args), std::move(label),
                              hub_instance_id, std::move(ns), std::move(exported_dir),
                              std::move(client_request)),
      remote_controller_(std::move(remote_controller)),
      container_(std::move(container)),
      termination_reason_(TerminationReason::UNKNOWN) {
  // Forward termination callbacks from the remote component over the bridge.
  remote_controller_.events().OnTerminated = [this](int64_t return_code,
                                                    TerminationReason termination_reason) mutable {
    // Propagate the events to the external proxy.
    if (on_terminated_event_) {
      on_terminated_event_(return_code, termination_reason);
    }

    SendOnTerminationEvent(return_code, termination_reason);
    remote_controller_.events().OnTerminated = nullptr;
    container_->ExtractComponent(this);
  };

  remote_controller_.events().OnDirectoryReady = [this] { SendOnDirectoryReadyEvent(); };

  remote_controller_.set_error_handler([this](zx_status_t status) {
    if (remote_controller_.events().OnTerminated != nullptr) {
      remote_controller_.events().OnTerminated(-1, TerminationReason::UNKNOWN);
    }
  });
  // The destructor of the temporary returned by ExtractComponent destroys
  // |this| at the end of the previous statement.

  hub()->AddIncomingServices(this->incoming_services());
}

ComponentBridge::~ComponentBridge() {
  if (remote_controller_.events().OnTerminated) {
    SendOnTerminationEvent(-1, termination_reason_);
  }
}

void ComponentBridge::SetParentJobId(const std::string& id) { hub()->SetJobId(id); }

void ComponentBridge::Kill() { remote_controller_->Kill(); }

void ComponentBridge::SetTerminationReason(TerminationReason termination_reason) {
  termination_reason_ = termination_reason;
}

}  // namespace component
