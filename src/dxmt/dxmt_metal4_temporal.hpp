#pragma once

#include "Metal.hpp"
#include <mutex>

namespace dxmt {

// Device-scoped state for temporal upscaling through Metal 4.
//
// An id<MTL4CommandBuffer> cannot be committed to the Metal 3 command queue, so
// a Metal 4 encode needs a second command queue beside CommandQueue. Apple
// documents ordering work across the two with MTLEvents belonging to the same
// device ("you can add signal and wait commands to any combination of
// MTL4CommandQueue and MTLCommandQueue instances that all belong to the same
// Metal device"), which is what scene_event/done_event below are for.
//
// Queue, compiler and synchronization events live with the device. Command
// allocators live with individual chunks so their memory is reclaimed only
// after GPU completion, without an additional CPU wait.
class Metal4Temporal {
public:
  // Whether this GPU can do temporal scaling compatible with Metal 4. False on
  // anything older than macOS 26, or on a GPU MetalFX does not support. This
  // reports capability only; initialize() can still fail to allocate objects.
  bool available() const {
    return supported_;
  }

  WMT::MTL4CommandQueue &queue() {
    return queue_;
  }

  WMT::MTL4Compiler &compiler() {
    return compiler_;
  }

  // Signalled on the Metal 3 stream once the scaler's inputs are written, and
  // waited on by the Metal 4 stream before it runs.
  WMT::Event &scene_event() {
    return scene_event_;
  }

  // Signalled by the Metal 4 stream once the scaler is done, and waited on by
  // the Metal 3 stream before anything reads the output.
  WMT::Event &done_event() {
    return done_event_;
  }

  // A fresh value for one encode. Both events share this counter, which is safe
  // precisely because they are distinct objects -- see the note at the encode.
  uint64_t next_value() const {
    return ++value_;
  }

  explicit Metal4Temporal(WMT::Device device)
      : device_(device), supported_(device.supportsFXTemporalScalerMetal4()) {}

  // Capability queries must not create a second queue. Initialize only for an
  // actual MetalFX 4 scaler request; publish either all objects or none.
  bool initialize() {
    if (!supported_)
      return false;
    std::call_once(initialize_once_, [this] {
      auto compiler = device_.newMTL4Compiler();
      auto queue = device_.newMTL4CommandQueue();
      if (!compiler || !queue)
        return;
      // Distinct same-device events: sharing one would satisfy the reverse wait
      // before Metal 4 finishes. Publication happens only after both exist.
      auto scene = device_.newEvent();
      auto done = device_.newEvent();
      if (!scene || !done)
        return;
      compiler_ = std::move(compiler);
      queue_ = std::move(queue);
      scene_event_ = std::move(scene);
      done_event_ = std::move(done);
      initialized_ = true;
    });
    return initialized_;
  }

private:
  WMT::Reference<WMT::Device> device_;
  const bool supported_;
  std::once_flag initialize_once_;
  bool initialized_ = false;
  WMT::Reference<WMT::MTL4CommandQueue> queue_;
  WMT::Reference<WMT::MTL4Compiler> compiler_;
  WMT::Reference<WMT::Event> scene_event_;
  WMT::Reference<WMT::Event> done_event_;
  mutable uint64_t value_ = 0;
};

} // namespace dxmt
