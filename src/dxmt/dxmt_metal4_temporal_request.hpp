#pragma once

#include "Metal.hpp"

namespace dxmt {

// Prepare every explicit fallible Metal 4 encode allocation before accepting
// the API request. The command buffer is private to this immediate-context
// request, not shared with another frame or replayable deferred command list.
struct Metal4TemporalRequest {
  WMT::Reference<WMT::MTL4CommandAllocator> allocator;
  WMT::Reference<WMT::MTL4CommandBuffer> command_buffer;
  WMT::Reference<WMT::ResidencySet> residency;

  bool prepare(WMT::Device device, WMT::Fence fence) {
    allocator = device.newMTL4CommandAllocator();
    command_buffer = device.newMTL4CommandBuffer();
    WMT::Reference<WMT::Error> error;
    residency = device.newResidencySet(5, error);
    if (!allocator || !command_buffer || !residency)
      return false;
    command_buffer.beginCommandBuffer(allocator);
    auto host = command_buffer.computeCommandEncoder();
    if (!host) {
      command_buffer.endCommandBuffer();
      return false;
    }
    host.updateFence(fence, (1ull << 27) | (1ull << 28));
    host.endEncoding();
    return true;
  }
};

} // namespace dxmt
