// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "benchmarks.h"

#include <inttypes.h>
#include <stdio.h>

#include <fbl/function.h>
#include <lib/async/cpp/task.h>
#include <trace-engine/buffer_internal.h>
#include <trace-engine/instrumentation.h>
#include <trace-vthread/event_vthread.h>
#include <trace/event.h>

#include "handler.h"
#include "runner.h"

namespace {

using Benchmark = fbl::Function<void()>;

class Runner {
public:
    Runner(bool enabled, const BenchmarkSpec* spec)
        : enabled_(enabled), spec_(spec) {}

    void Run(const char* name, Benchmark benchmark) {
        if (enabled_) {
            // The trace engine needs to run in its own thread in order to
            // process buffer full requests in streaming mode while the
            // benchmark is running. Note that records will still get lost
            // if the engine thread is not scheduled frequently enough. This
            // is a stress test so all the app is doing is filling the trace
            // buffer. :-)
            async::Loop loop(&kAsyncLoopConfigNoAttachToThread);
            BenchmarkHandler handler(&loop, spec_->mode, spec_->buffer_size);

            loop.StartThread("trace-engine loop", nullptr);

            RunAndMeasure(
                name, spec_->name, spec_->num_iterations, benchmark,
                [&handler]() { handler.Start(); },
                [&handler]() { handler.Stop(); });

            loop.Quit();
            loop.JoinThreads();
        } else {
            // For the disabled benchmarks we just use the default number
            // of iterations.
            RunAndMeasure(
                name, spec_->name, benchmark, []() {}, []() {});
        }
    }

private:
    const bool enabled_;
    // nullptr if |!enabled_|.
    const BenchmarkSpec* spec_;
};

#define RUN_NILADIC_DURATION_TEST(test_name, DURATION_MACRO, category_literal)     \
    do {                                                                           \
        runner.Run(#DURATION_MACRO " macro with " test_name ": " category_literal, \
                   [] {                                                            \
                       DURATION_MACRO(category_literal, "name");                   \
                   });                                                             \
    } while (0)

#define RUN_DURATION_TEST(test_name, DURATION_MACRO, category_literal, ...)        \
    do {                                                                           \
        runner.Run(#DURATION_MACRO " macro with " test_name ": " category_literal, \
                   [] {                                                            \
                       DURATION_MACRO(category_literal, "name",                    \
                                      __VA_ARGS__);                                \
                   });                                                             \
    } while (0)

#define DURATION_TEST(DURATION_MACRO, category_literal)          \
    RUN_NILADIC_DURATION_TEST("0 arguments",                     \
                              DURATION_MACRO, category_literal); \
                                                                 \
    RUN_DURATION_TEST("1 int32 argument",                        \
                      DURATION_MACRO, category_literal,          \
                      "k1", 1);                                  \
                                                                 \
    RUN_DURATION_TEST("1 double argument",                       \
                      DURATION_MACRO, category_literal,          \
                      "k1", 1.);                                 \
                                                                 \
    RUN_DURATION_TEST("1 string argument",                       \
                      DURATION_MACRO, category_literal,          \
                      "k1", "string1");                          \
                                                                 \
    RUN_DURATION_TEST("4 int32 arguments",                       \
                      DURATION_MACRO, category_literal,          \
                      "k1", 1, "k2", 2, "k3", 3, "k4", 4);       \
                                                                 \
    RUN_DURATION_TEST("4 double arguments",                      \
                      DURATION_MACRO, category_literal,          \
                      "k1", 1., "k2", 2., "k3", 3., "k4", 4.);   \
                                                                 \
    RUN_DURATION_TEST("4 string arguments",                      \
                      DURATION_MACRO, category_literal,          \
                      "k1", "string1", "k2", "string2",          \
                      "k3", "string3", "k4", "string4");         \
                                                                 \
    RUN_DURATION_TEST("8 int32 arguments",                       \
                      DURATION_MACRO, category_literal,          \
                      "k1", 1, "k2", 2, "k3", 3, "k4", 4,        \
                      "k5", 5, "k6", 6, "k7", 7, "k8", 8);       \
                                                                 \
    RUN_DURATION_TEST("8 double arguments",                      \
                      DURATION_MACRO, category_literal,          \
                      "k1", 1., "k2", 2., "k3", 3., "k4", 4.,    \
                      "k5", 4., "k6", 5., "k7", 7., "k8", 8.);   \
                                                                 \
    RUN_DURATION_TEST("8 string arguments",                      \
                      DURATION_MACRO, category_literal,          \
                      "k1", "string1", "k2", "string2",          \
                      "k3", "string3", "k4", "string4",          \
                      "k5", "string5", "k6", "string6",          \
                      "k7", "string7", "k8", "string8");

void RunBenchmarks(bool tracing_enabled, const BenchmarkSpec* spec) {
    Runner runner(tracing_enabled, spec);

    runner.Run("is enabled", [] {
        trace_is_enabled();
    });

    runner.Run("is category enabled", [] {
        trace_is_category_enabled("+enabled");
    });

    if (tracing_enabled) {
        runner.Run("is category enabled for disabled category", [] {
            trace_is_category_enabled("-disabled");
        });
    }

    runner.Run("acquire / release context", [] {
        trace_context_t* context = trace_acquire_context();
        if (unlikely(context))
            trace_release_context(context);
    });

    runner.Run("acquire / release context for category", [] {
        trace_string_ref_t category_ref;
        trace_context_t* context = trace_acquire_context_for_category(
            "+enabled", &category_ref);
        if (unlikely(context))
            trace_release_context(context);
    });

    if (tracing_enabled) {
        runner.Run("acquire / release context for disabled category", [] {
            trace_string_ref_t category_ref;
            trace_context_t* context = trace_acquire_context_for_category(
                "-disabled", &category_ref);
            ZX_DEBUG_ASSERT(!context);
        });
    }

    DURATION_TEST(TRACE_DURATION_BEGIN, "+enabled");
    DURATION_TEST(TRACE_DURATION, "+enabled");

    runner.Run("TRACE_VTHREAD_DURATION_BEGIN macro with 0 arguments", [] {
        TRACE_VTHREAD_DURATION_BEGIN("+enabled", "name", "vthread", 1, zx_ticks_get());
    });

    if (tracing_enabled) {
        DURATION_TEST(TRACE_DURATION_BEGIN, "-disabled");
        DURATION_TEST(TRACE_DURATION, "-disabled");

        runner.Run("TRACE_VTHREAD_DURATION_BEGIN macro with 0 arguments for disabled category", [] {
            TRACE_VTHREAD_DURATION_BEGIN("-disabled", "name", "vthread", 1, zx_ticks_get());
        });
    }
}

} // namespace

void RunTracingDisabledBenchmarks() {
    static const BenchmarkSpec spec = {
        "disabled",
        TRACE_BUFFERING_MODE_ONESHOT, // unused
        0,
        kDefaultRunIterations,
    };
    RunBenchmarks(false, &spec);
}

void RunTracingEnabledBenchmarks(const BenchmarkSpec* spec) {
    RunBenchmarks(true, spec);
}
