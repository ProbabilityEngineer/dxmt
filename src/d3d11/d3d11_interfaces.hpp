#pragma once
#include "com/com_guid.hpp"
#include "d3d11.h"

struct MTL_TEMPORAL_UPSCALE_D3D11_DESC {
  UINT InputContentWidth; // can be 0, which means full width
  UINT InputContentHeight; // can be 0, which means full height
  BOOL AutoExposure;
  BOOL InReset;
  BOOL DepthReversed;
  BOOL MotionVectorInDisplayRes;
  ID3D11Resource *Color;
  ID3D11Resource *Depth;
  ID3D11Resource *MotionVector;
  ID3D11Resource *Output;
  FLOAT MotionVectorScaleX;
  FLOAT MotionVectorScaleY;
  FLOAT PreExposure;
  ID3D11Resource *ExposureTexture;
  FLOAT JitterOffsetX;
  FLOAT JitterOffsetY;
};

DEFINE_COM_INTERFACE("43ace3ce-1956-448b-a4eb-aee68bdeb283",
                     IMTLD3D11ContextExt)
    : public IUnknown{
   virtual void STDMETHODCALLTYPE TemporalUpscale(const MTL_TEMPORAL_UPSCALE_D3D11_DESC *pDesc) = 0;
   virtual void STDMETHODCALLTYPE BeginUAVOverlap() = 0;
   virtual void STDMETHODCALLTYPE EndUAVOverlap() = 0;
};

// Which generation of the framework's temporal scaler a caller wants.
//
// MTL4FXTemporalScaler refines
// MTLFXTemporalScalerBase, the same protocol MTLFXTemporalScaler conforms to,
// so every field of MTL_TEMPORAL_UPSCALE_D3D11_DESC means exactly what it
// already meant. The generation only selects which object is created and which
// command buffer it encodes into -- a Metal 3 command buffer, or for MetalFX 4
// an id<MTL4CommandBuffer> committed to an id<MTL4CommandQueue>.
//
// It is passed as a separate argument to IMTLD3D11ContextExt2 rather than added
// to MTL_TEMPORAL_UPSCALE_D3D11_DESC, which leaves that struct byte-for-byte as
// it always was. A descriptor is passed by pointer, so growing it would force
// every already-compiled caller to be rebuilt and would read past the end of an
// older caller's allocation. MTL_FX_GENERATION_METAL3 is 0, so an unspecified
// request means Metal 3.
enum MTL_FX_GENERATION : UINT {
  MTL_FX_GENERATION_METAL3 = 0,
  MTL_FX_GENERATION_METAL4 = 1,
};

typedef enum MTL_FEATURE {
  MTL_FEATURE_METALFX_TEMPORAL_SCALER = 0,
  // Whether the GPU supports temporal scaling compatible with Metal 4
  // (id<MTL4FXTemporalScaler>, which encodes into an id<MTL4CommandBuffer>).
  // Answers FALSE -- not an error -- when the host is older than macOS 26 or
  // the GPU does not support it, in which case the caller should keep using
  // MTL_FEATURE_METALFX_TEMPORAL_SCALER. Querying this tells a caller whether
  // setting MTL_FX_GENERATION_METAL4 is worth attempting; it does not select it.
  MTL_FEATURE_METALFX_TEMPORAL_SCALER_METAL4 = 1,
} MTL_FEATURE;

DEFINE_COM_INTERFACE("19a8e35a-38be-418f-94e3-9f7323936870", IMTLD3D11ContextExt1) : public IMTLD3D11ContextExt {
  virtual HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
      MTL_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize
  ) = 0;
};

DEFINE_COM_INTERFACE("6523d273-9915-44b3-a5f0-f69a654da1f8", IMTLD3D11ContextExt2) : public IMTLD3D11ContextExt1 {
   // Same as IMTLD3D11ContextExt::TemporalUpscale, with the scaler generation
   // chosen explicitly. Both generations share one descriptor and one
   // implementation; only the scaler object and the command buffer it encodes
   // into differ, and a caller may use either in the same session.
   //
   // Requesting MTL_FX_GENERATION_METAL4 never falls back to MetalFX 3. If MetalFX
   // 4 is unavailable, or if the scaler cannot be created for the requested
   // formats and sizes, this call reports the failure and does nothing -- the
   // output texture is left untouched. Callers should query
   // MTL_FEATURE_METALFX_TEMPORAL_SCALER_METAL4 first. Silently substituting
   // MetalFX 3 would make the caller's validation and A/B comparison meaningless.
   virtual void STDMETHODCALLTYPE TemporalUpscaleEx(
       const MTL_TEMPORAL_UPSCALE_D3D11_DESC *pDesc, MTL_FX_GENERATION FxGeneration
   ) = 0;
};

DEFINE_COM_INTERFACE("efc77ae6-2179-4c0a-b844-7661ca0dcde7", IMTLD3D11DeviceExt)
    : public IUnknown {
  virtual void STDMETHODCALLTYPE SetShaderExtensionSlot(UINT Slot) = 0;
};
