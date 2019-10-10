// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/ui/root_presenter/factory_reset_manager.h"

#include <fuchsia/recovery/cpp/fidl_test_base.h>
#include <lib/async/time.h>
#include <lib/fidl/cpp/binding_set.h>
#include <zircon/status.h>

#include "gtest/gtest.h"
#include "src/lib/component/cpp/startup_context.h"
#include "src/lib/component/cpp/testing/test_with_context.h"

namespace root_presenter {
namespace testing {

class FakeFactoryReset : public fuchsia::recovery::testing::FactoryReset_TestBase {
 public:
  void NotImplemented_(const std::string& name) final {}

  fidl::InterfaceRequestHandler<fuchsia::recovery::FactoryReset> GetHandler(
      async_dispatcher_t* dispatcher = nullptr) {
    return [this, dispatcher](fidl::InterfaceRequest<fuchsia::recovery::FactoryReset> request) {
      bindings_.AddBinding(this, std::move(request), dispatcher);
    };
  }

  void Reset(ResetCallback callback) override {
    callback(ZX_OK);
    triggered_ = true;
  }

  bool triggered() const { return triggered_; }

 private:
  bool triggered_ = false;

  fidl::BindingSet<fuchsia::recovery::FactoryReset> bindings_;
};

class FactoryResetManagerTest : public component::testing::TestWithContext {
 public:
  void SetUp() final {
    controller().AddService(factory_reset_.GetHandler());

    startup_context_ = TakeContext();
    factory_reset_manager_ = std::make_unique<FactoryResetManager>(startup_context_.get());
  }

  bool triggered() const { return factory_reset_.triggered(); }

 protected:
  std::unique_ptr<FactoryResetManager> factory_reset_manager_;

 private:
  std::unique_ptr<component::StartupContext> startup_context_;
  FakeFactoryReset factory_reset_;
};

TEST_F(FactoryResetManagerTest, FactoryResetButtonPressedAndReleased) {
  EXPECT_FALSE(factory_reset_manager_->countdown_started());

  fuchsia::ui::input::MediaButtonsReport report;
  report.reset = true;
  factory_reset_manager_->OnMediaButtonReport(report);
  EXPECT_TRUE(factory_reset_manager_->countdown_started());

  // Factory reset should cancel if the button is released.
  report.reset = false;
  factory_reset_manager_->OnMediaButtonReport(report);
  EXPECT_FALSE(factory_reset_manager_->countdown_started());

  RunLoopFor(kCountdownDuration);
  RunLoopUntilIdle();
  EXPECT_FALSE(triggered());
}

TEST_F(FactoryResetManagerTest, FactoryResetButtonHeldAndTrigger) {
  EXPECT_FALSE(factory_reset_manager_->countdown_started());

  fuchsia::ui::input::MediaButtonsReport report;
  report.reset = true;

  factory_reset_manager_->OnMediaButtonReport(report);
  EXPECT_TRUE(factory_reset_manager_->countdown_started());

  RunLoopFor(kCountdownDuration);
  RunLoopUntilIdle();
  EXPECT_TRUE(triggered());
}

TEST_F(FactoryResetManagerTest, WatchHandler) {
  const int scheduled_reset_time = 200;

  fuchsia::recovery::ui::FactoryResetCountdownState inputState;
  fuchsia::recovery::ui::FactoryResetCountdownState outputState;

  FactoryResetManager::WatchHandler watchHandler(inputState);

  bool watchReturned = false;

  auto watchCallback =
      [&outputState, &watchReturned](fuchsia::recovery::ui::FactoryResetCountdownState newState) {
        watchReturned = true;
        newState.Clone(&outputState);
      };

  watchHandler.Watch(watchCallback);

  // Initial watch should return immediately, with no scheduled reset.
  EXPECT_TRUE(watchReturned);
  EXPECT_FALSE(outputState.has_scheduled_reset_time());
  watchReturned = false;

  watchHandler.Watch(watchCallback);

  // Subsequent watch should hang until state changes.
  EXPECT_FALSE(watchReturned);

  inputState.set_scheduled_reset_time(scheduled_reset_time);
  watchHandler.OnStateChange(inputState);

  // On the state change, the watch should return with the new scheduled reset time.
  EXPECT_TRUE(watchReturned);
  EXPECT_TRUE(outputState.has_scheduled_reset_time());
  EXPECT_EQ(outputState.scheduled_reset_time(), scheduled_reset_time);
  watchReturned = false;

  inputState.clear_scheduled_reset_time();
  watchHandler.OnStateChange(inputState);

  watchHandler.Watch(watchCallback);

  // When state changes before watch is called, watch should return immediately.
  EXPECT_TRUE(watchReturned);
  EXPECT_FALSE(outputState.has_scheduled_reset_time());
}

}  // namespace testing
}  // namespace root_presenter
