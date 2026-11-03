#include "dxmt_scaler.hpp"

namespace dxmt {

void
SpatialScaler::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
}

void
SpatialScaler::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
}

SpatialScaler::SpatialScaler(WMT::Device device, const WMTFXSpatialScalerInfo &info) {
  scaler_ = device.newSpatialScaler(info);
  fence_ = device.newFence();
};

void
TemporalScaler::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
}

void
TemporalScaler::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
}

TemporalScaler::TemporalScaler(WMT::Device device, const WMTFXTemporalScalerInfo &info)
    : TemporalScaler(device, info, FXGeneration::Metal3, {}) {
}

TemporalScaler::TemporalScaler(WMT::Device device, const WMTFXTemporalScalerInfo &info, FXGeneration generation,
                               WMT::Reference<WMT::MTL4Compiler> compiler)
    : compiler_(compiler), generation_(generation) {
  if (generation == FXGeneration::Metal4) {
    scaler_ = device.newTemporalScalerMetal4(compiler, info);
  } else {
    scaler_ = device.newTemporalScaler(info);
  }
  fence_ = device.newFence();
}

} // namespace dxmt
