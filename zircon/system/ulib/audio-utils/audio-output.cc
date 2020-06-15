// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <zircon/device/audio.h>

#include <algorithm>
#include <memory>

#include <audio-utils/audio-output.h>
#include <audio-utils/audio-stream.h>
#include <fbl/algorithm.h>
#include <fbl/alloc_checker.h>

namespace audio {
namespace utils {

std::unique_ptr<AudioOutput> AudioOutput::Create(uint32_t dev_id) {
  fbl::AllocChecker ac;
  std::unique_ptr<AudioOutput> res(new (&ac) AudioOutput(dev_id));
  if (!ac.check())
    return nullptr;
  return res;
}

std::unique_ptr<AudioOutput> AudioOutput::Create(const char* dev_path) {
  fbl::AllocChecker ac;
  std::unique_ptr<AudioOutput> res(new (&ac) AudioOutput(dev_path));
  if (!ac.check())
    return nullptr;
  return res;
}

zx_status_t AudioOutput::Play(AudioSource& source) {
  zx_status_t res;

  if (source.finished())
    return ZX_OK;

  AudioSource::Format format;
  res = source.GetFormat(&format);
  if (res != ZX_OK) {
    printf("Failed to get source's format (res %d)\n", res);
    return res;
  }

  res = SetFormat(format.frame_rate, format.channels, format.channels_to_use_bitmask,
                  format.sample_format);
  if (res != ZX_OK) {
    printf("Failed to set source format [%u Hz, %hu Chan, %016lx Mask, %08x fmt] (res %d)\n",
           format.frame_rate, format.channels, format.channels_to_use_bitmask, format.sample_format,
           res);
    return res;
  }

  // ALSA under QEMU required huge buffers.
  //
  // TODO(johngro) : Add the ability to determine what type of read-ahead the
  // HW is going to require so we can adjust our buffer size to what the HW
  // requires, not what ALSA under QEMU requires.
  res = GetBuffer(480 * 20 * 3, 3);
  if (res != ZX_OK) {
    printf("Failed to set output format (res %d)\n", res);
    return res;
  }

  memset(rb_virt_, 0, rb_sz_);

  auto buf = reinterpret_cast<uint8_t*>(rb_virt_);
  uint32_t rd, wr;
  uint32_t playout_rd, playout_amt;
  bool started = false;
  rd = wr = 0;
  playout_rd = playout_amt = 0;

  while (true) {
    // Top up the buffer.  In theory, we should only need to loop 2 times in
    // order to handle a ring discontinuity
    for (uint32_t i = 0; i < 2; ++i) {
      uint32_t space = (rb_sz_ + rd - wr - 1) % rb_sz_;
      uint32_t todo = std::min(space, rb_sz_ - wr);
      ZX_DEBUG_ASSERT(space < rb_sz_);

      if (!todo)
        break;

      if (source.finished()) {
        memset(buf + wr, 0, todo);
        zx_cache_flush(buf + wr, todo, ZX_CACHE_FLUSH_DATA);

        wr += todo;
      } else {
        uint32_t done;
        res = source.GetFrames(buf + wr, std::min(space, rb_sz_ - wr), &done);
        if (res != ZX_OK) {
          printf("Error packing frames (res %d)\n", res);
          break;
        }
        zx_cache_flush(buf + wr, done, ZX_CACHE_FLUSH_DATA);
        wr += done;

        if (source.finished()) {
          playout_rd = rd;
          playout_amt = (rb_sz_ + wr - rd) % rb_sz_;

          // We have just become finished.  Reset the loop counter and
          // start over, this time filling with as much silence as we
          // can.
          i = 0;
        }
      }

      if (wr < rb_sz_)
        break;

      ZX_DEBUG_ASSERT(wr == rb_sz_);
      wr = 0;
    }

    if (res != ZX_OK)
      break;

    // If we have not started yet, do so.
    if (!started) {
      res = StartRingBuffer();
      if (res != ZX_OK) {
        printf("Failed to start ring buffer!\n");
        break;
      }
      started = true;
    }

    auto position =
        audio_fidl::RingBuffer::Call::WatchClockRecoveryPositionInfo(zx::unowned_channel(rb_ch_));

    rd = position->position_info.position;

    // rd has moved.  If the source has finished and rd has moved at least
    // the playout distance, we are finsihed.
    if (source.finished()) {
      uint32_t dist = (rb_sz_ + rd - playout_rd) % rb_sz_;

      if (dist >= playout_amt)
        break;

      playout_amt -= dist;
      playout_rd = rd;
    }
  }

  if (res == ZX_OK) {
    // We have already let the DMA engine catch up, but we still need to
    // wait for the fifo to play out.  For now, just hard code this as
    // 30uSec.
    //
    // TODO: base this on the start time and the number of frames queued
    // instead of just making a number up.
    zx_nanosleep(zx_deadline_after(ZX_MSEC(30)));
  }

  zx_status_t stop_res = StopRingBuffer();
  if (res == ZX_OK)
    res = stop_res;

  return res;
}

}  // namespace utils

}  // namespace audio
