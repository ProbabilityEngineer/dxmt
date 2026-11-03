#pragma once

#include "Metal.hpp"
#include <atomic>

namespace dxmt {

class SpatialScaler {
public:
  void incRef();
  void decRef();

  SpatialScaler(WMT::Device device, const WMTFXSpatialScalerInfo &info);

  WMT::FXSpatialScaler scaler() {
    return scaler_;
  }

  WMT::Fence fence() {
    return fence_;
  }

private:
  WMT::Reference<WMT::FXSpatialScaler> scaler_;
  WMT::Reference<WMT::Fence> fence_;
  std::atomic<uint32_t> refcount_;
};

// Which generation of the framework's temporal scaler an encode uses.
//
// MTL4FXTemporalScaler refines
// MTLFXTemporalScalerBase, the same protocol MTLFXTemporalScaler conforms to,
// so every scaler property and every WMTFXTemporalScalerProps field is
// identical. The generation only decides which object is created and which
// command buffer it encodes into -- Metal 3 for Metal3, and for Metal4 an
// id<MTL4CommandBuffer> committed to an id<MTL4CommandQueue>.
//
// The values mirror MTL_FX_GENERATION in the D3D11 vendor extension, which is
// what a caller passes to IMTLD3D11ContextExt3::TemporalUpscaleEx. Metal3 is 0
// there so an unspecified generation means Metal 3, and must stay 0 here: the
// implementation converts between the two enums directly when keying the
// scaler cache.
enum class FXGeneration : uint32_t {
  Metal3 = 0,
  Metal4 = 1,
};

class TemporalScaler {
public:
  void incRef();
  void decRef();

  TemporalScaler(WMT::Device device, const WMTFXTemporalScalerInfo &info);
  // Metal 4 scalers are created through an MTL4Compiler. The compiler is kept
  // alive for the scaler's lifetime rather than trusted to MetalFX, so the
  // scaler cannot outlive something it was built from.
  TemporalScaler(WMT::Device device, const WMTFXTemporalScalerInfo &info, FXGeneration generation,
                 WMT::Reference<WMT::MTL4Compiler> compiler);

  WMT::FXTemporalScaler scaler() {
    return scaler_;
  }

  WMT::Fence fence() {
    return fence_;
  }

  FXGeneration generation() const {
    return generation_;
  }

private:
  WMT::Reference<WMT::FXTemporalScaler> scaler_;
  WMT::Reference<WMT::MTL4Compiler> compiler_;
  WMT::Reference<WMT::Fence> fence_;
  FXGeneration generation_ = FXGeneration::Metal3;
  std::atomic<uint32_t> refcount_;
};

} // namespace dxmt
