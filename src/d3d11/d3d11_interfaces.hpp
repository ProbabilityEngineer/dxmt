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
// It is passed as a separate argument to IMTLD3D11ContextExt3 rather than added
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

// Replaces the private void-returning Ext2 contract. Its old IID is deliberately
// not exposed: mixing old/new DLLs must fail QueryInterface, not read an
// undefined return register as a status.
DEFINE_COM_INTERFACE("e6fc275c-395c-4134-afaf-5826ddc07e4e", IMTLD3D11ContextExt3) : public IMTLD3D11ContextExt1 {
   // Immediate contexts only. S_OK means the request was accepted, with scaler,
   // fence and explicit Metal 4 per-request allocations prepared. It does NOT
   // mean GPU completion or verified pixels. No CPU/GPU wait is introduced.
   // Failure queues no upscale and leaves Output untouched by this request.
   // E_INVALIDARG: missing/invalid resources, extent or generation.
   // DXGI_ERROR_UNSUPPORTED: requested generation unavailable.
   // E_OUTOFMEMORY: fence or Metal 4 request allocation failed.
   // E_FAIL: native initialization/scaler creation failed.
   // The requested generation is never silently substituted.
   virtual HRESULT STDMETHODCALLTYPE TemporalUpscaleEx(
       const MTL_TEMPORAL_UPSCALE_D3D11_DESC *pDesc, MTL_FX_GENERATION FxGeneration
   ) = 0;
};

DEFINE_COM_INTERFACE("efc77ae6-2179-4c0a-b844-7661ca0dcde7", IMTLD3D11DeviceExt)
    : public IUnknown {
  virtual void STDMETHODCALLTYPE SetShaderExtensionSlot(UINT Slot) = 0;
};
