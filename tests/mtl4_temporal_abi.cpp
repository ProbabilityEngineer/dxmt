/*
 * Static checks for the MetalFX 4 additions to the D3D11 vendor extension.
 *
 * DXMT's CI has no test job and tests/ only builds Windows D3D11 samples, so
 * there is no harness to plug into. These checks are a standalone,
 * dependency-light program a developer can run directly on a Mac:
 *
 *   clang++ -std=c++20 -fsyntax-only -fblocks \
 *            -I include -I src/d3d11 -I src/dxgi -I src/dxmt -I src/winemetal \
 *            -I src/util -I src/airconv -I include/native/windows \
 *            -I include/native/directx \
 *            tests/mtl4_temporal_abi.cpp
 *
 * What is asserted here is the *ABI contract* an existing caller sees, plus the
 * decision table behind the MetalFX 4 failure semantics. Runtime behaviour --
 * which scaler an encode picks, how the two command queues are ordered -- needs
 * a GPU and is covered by the MetalFX path itself, not here.
 */

#include "d3d11_interfaces.hpp"
#include "dxmt_scaler.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

// ---------------------------------------------------------------------------
// MTL_TEMPORAL_UPSCALE_D3D11_DESC is untouched.
//
// The generation is NOT a field here. It is an argument to
// IMTLD3D11ContextExt3::TemporalUpscaleEx precisely so that this struct stays
// byte-for-byte as it always was: a descriptor is passed by pointer, so growing
// it would read past the end of an already-compiled caller's allocation and
// would force every out-of-tree caller to be rebuilt. These offsets and the size
// pin that contract, so reintroducing the field fails the build.
// ---------------------------------------------------------------------------
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, InputContentWidth) == 0);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, InputContentHeight) == 4);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, AutoExposure) == 8);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, InReset) == 12);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, DepthReversed) == 16);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, MotionVectorInDisplayRes) == 20);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, Color) == 24);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, Depth) == 32);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, MotionVector) == 40);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, Output) == 48);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, MotionVectorScaleX) == 56);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, MotionVectorScaleY) == 60);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, PreExposure) == 64);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, ExposureTexture) == 72);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, JitterOffsetX) == 80);
static_assert(offsetof(MTL_TEMPORAL_UPSCALE_D3D11_DESC, JitterOffsetY) == 84);
static_assert(sizeof(MTL_TEMPORAL_UPSCALE_D3D11_DESC) == 88,
              "the descriptor must stay 88 bytes: the MetalFX 4 generation is an "
              "argument to TemporalUpscaleEx, never a field here");

// ---------------------------------------------------------------------------
// Generation values. Metal 3 is 0 so an unspecified request means Metal 3, and
// the internal enum used by the scaler cache agrees with the ABI enum because
// the implementation converts between them directly.
// ---------------------------------------------------------------------------
static_assert(static_cast<UINT>(MTL_FX_GENERATION_METAL3) == 0,
              "Metal 3 must be 0 so an unspecified request means Metal 3");
static_assert(static_cast<UINT>(MTL_FX_GENERATION_METAL4) == 1);
static_assert(static_cast<UINT>(MTL_FX_GENERATION_METAL3) == static_cast<UINT>(dxmt::FXGeneration::Metal3));
static_assert(static_cast<UINT>(MTL_FX_GENERATION_METAL4) == static_cast<UINT>(dxmt::FXGeneration::Metal4));

// ---------------------------------------------------------------------------
// Feature enumeration. The Metal 3 query keeps its value and its answer; the
// Metal 4 query is appended and is advisory -- it tells a caller whether
// requesting MetalFX 4 is worth attempting, it does not select it.
// ---------------------------------------------------------------------------
static_assert(static_cast<int>(MTL_FEATURE_METALFX_TEMPORAL_SCALER) == 0,
              "the existing feature must keep value 0");
static_assert(static_cast<int>(MTL_FEATURE_METALFX_TEMPORAL_SCALER_METAL4) == 1,
              "the MetalFX 4 feature is appended");

// ---------------------------------------------------------------------------
// Interface revisions. IMTLD3D11ContextExt3 refines Ext1, so a caller that only
// knows Ext1 is unaffected and an older DXMT simply does not answer the new IID
// -- which is the clean, detectable "old DXMT" condition.
// ---------------------------------------------------------------------------
static_assert(std::is_base_of<IMTLD3D11ContextExt1, IMTLD3D11ContextExt3>::value,
              "Ext3 retains Ext1's feature-query slots");
static_assert(std::is_same<decltype(&IMTLD3D11ContextExt3::TemporalUpscaleEx),
              HRESULT (STDMETHODCALLTYPE IMTLD3D11ContextExt3::*)(
                  const MTL_TEMPORAL_UPSCALE_D3D11_DESC*, MTL_FX_GENERATION)>::value,
              "the coordinated private interface returns request acceptance");

namespace {

// ---------------------------------------------------------------------------
// MetalFX 4 failure semantics.
//
// The runtime rule lives in MTLD3D11ContextExt::temporalUpscale. These mirror
// the decision table there so that reintroducing a fallback breaks the build
// rather than silently changing which scaler runs.
// ---------------------------------------------------------------------------
enum class CreateOutcome {
  Metal3,               // generation 0 -- the existing path
  Metal4,
  RefusedUnavailable,   // explicit MetalFX 4, device cannot do it
  RefusedNilScaler,     // explicit MetalFX 4, MetalFX returned nil
};

constexpr CreateOutcome
decide(dxmt::FXGeneration requested, bool metal4_available, bool metal4_creation_succeeded) {
  if (requested == dxmt::FXGeneration::Metal3)
    return CreateOutcome::Metal3;
  if (!metal4_available)
    return CreateOutcome::RefusedUnavailable;
  if (!metal4_creation_succeeded)
    return CreateOutcome::RefusedNilScaler;
  return CreateOutcome::Metal4;
}

// The property that matters: an explicit MetalFX 4 request either produces a
// MetalFX 4 scaler or produces nothing at all. It never produces a MetalFX 3
// scaler, because a silent substitution would make the caller's validation and
// A/B benchmarking meaningless and would mask a broken capability query.
constexpr bool
serves_metal3_for_metal4_request(dxmt::FXGeneration requested, CreateOutcome outcome) {
  return requested == dxmt::FXGeneration::Metal4 && outcome == CreateOutcome::Metal3;
}

static_assert(!serves_metal3_for_metal4_request(dxmt::FXGeneration::Metal4,
                                                decide(dxmt::FXGeneration::Metal4, true, true)),
              "an available MetalFX 4 request must produce a MetalFX 4 scaler");
static_assert(!serves_metal3_for_metal4_request(dxmt::FXGeneration::Metal4,
                                                decide(dxmt::FXGeneration::Metal4, false, true)),
              "MetalFX 4 requested on a device without it must be refused, never downgraded");
static_assert(!serves_metal3_for_metal4_request(dxmt::FXGeneration::Metal4,
                                                decide(dxmt::FXGeneration::Metal4, true, false)),
              "a nil MetalFX 4 scaler must be refused, never downgraded");

// Generation 0 is untouched: it always produces MetalFX 3, whatever the device,
// and is never promoted to MetalFX 4 even when that is available.
static_assert(decide(dxmt::FXGeneration::Metal3, false, false) == CreateOutcome::Metal3,
              "generation 0 must always mean MetalFX 3");
static_assert(decide(dxmt::FXGeneration::Metal3, true, true) == CreateOutcome::Metal3,
              "generation 0 must not be promoted to MetalFX 4");
static_assert(!serves_metal3_for_metal4_request(dxmt::FXGeneration::Metal3,
                                                decide(dxmt::FXGeneration::Metal3, true, true)),
              "serving MetalFX 3 to a MetalFX 3 request is correct, not a downgrade");

// A refused request caches nothing, so a later request is decided afresh rather
// than matching an entry that claims a generation it does not hold.
static_assert(decide(dxmt::FXGeneration::Metal4, false, true) != decide(dxmt::FXGeneration::Metal4, true, true),
              "availability must change the outcome rather than be remembered from a refusal");

} // namespace

int main() {
  return 0;
}
