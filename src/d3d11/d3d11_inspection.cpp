#include "d3d11_inspection.hpp"
#include "log/log.hpp"

namespace dxmt {

MTLD3D11Inspection::MTLD3D11Inspection(WMT::Device pDevice)
    : m_device(pDevice) {
  // UMA does not imply tile-based deferred rendering. DXMT has no reliable
  // Metal query for this D3D11 architecture property.
  m_architectureInfo.TileBasedDeferredRenderer = FALSE;

  m_threading.DriverConcurrentCreates = TRUE;
  m_threading.DriverCommandLists = TRUE;

  /**
  MSL Specification 2.1: Metal does _not_ support double
  */
  m_doubles.DoublePrecisionFloatShaderOps = FALSE;

  m_d3d9Options.FullNonPow2TextureSupport = TRUE;
  m_d3d10Options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x = TRUE;

  m_d3d11Options.ClearView = TRUE;
  m_d3d11Options.ConstantBufferOffsetting = TRUE;
  m_d3d11Options.ConstantBufferPartialUpdate = TRUE;
  m_d3d11Options.MapNoOverwriteOnDynamicConstantBuffer = TRUE;
  /**
  https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400754-copyfromtexture?language=objc
  Copying data to overlapping regions within the same texture may result in
  unexpected behavior.

  https://learn.microsoft.com/en-us/windows/win32/direct3d11/using-direct3d-optional-features-to-supplement-direct3d-feature-levels
  TODO: ClearView, CopyWithOverlap, ConstantBufferPartialUpdate,
  ConstantBufferOffsetting, and MapNoOverwriteOnDynamicConstantBuffer must be
  all TRUE/FALSE. Overlapping copy needs special handling
  */
  // Metal overlapping texture copies have undefined behavior unless they are
  // explicitly emulated. DXMT does not currently provide that emulation.
  m_d3d11Options.CopyWithOverlap = FALSE;
  // ForcedSampleCount is consumed only by the UAV-only rendering path;
  // regular multisample RTV rendering does not apply this behavior.
  m_d3d11Options.MultisampleRTVWithForcedSampleCountOne = FALSE;
  m_d3d11Options.MapNoOverwriteOnDynamicBufferSRV = TRUE;
  /**
  https://github.com/gpuweb/gpuweb/issues/503#issuecomment-908973358
  */
  m_d3d11Options.UAVOnlyRenderingForcedSampleCount = TRUE;
  // DiscardResource/DiscardView are currently no-ops and do not reach Metal.
  m_d3d11Options.DiscardAPIsSeenByDriver = FALSE;
  m_d3d11Options.ExtendedDoublesShaderInstructions = FALSE;
  /**
  FIXME: This is questionable
  */
  m_d3d11Options.ExtendedResourceSharing = TRUE;
  // No DXBC SAD4 lowering path is currently implemented.
  m_d3d11Options.SAD4ShaderInstructions = FALSE;
  m_d3d11Options.FlagsForUpdateAndCopySeenByDriver = TRUE;
#ifdef DXMT_NO_PRIVATE_API
  m_d3d11Options.OutputMergerLogicOp = FALSE;
#else
  m_d3d11Options.OutputMergerLogicOp = TRUE;
#endif

  m_d3d11Options3.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer = TRUE;

  m_d3d11Options5.SharedResourceTier = D3D11_SHARED_RESOURCE_TIER_0; // TODO

  // use dxvk values
  m_gpuVirtualAddress.MaxGPUVirtualAddressBitsPerProcess = 40;
  m_gpuVirtualAddress.MaxGPUVirtualAddressBitsPerResource = 32;

  m_shaderCache.SupportFlags =
      D3D11_SHADER_CACHE_SUPPORT_AUTOMATIC_DISK_CACHE |
      D3D11_SHADER_CACHE_SUPPORT_AUTOMATIC_INPROC_CACHE;

  // DXBC minimum-precision declarations are parsed but not applied by the
  // translator; do not advertise 16-bit minimum-precision support.
  m_shaderMinPrecision.PixelShaderMinPrecision = 0;
  m_shaderMinPrecision.AllOtherShaderStagesMinPrecision = 0;

  m_d3d9Shadow.SupportsDepthAsTextureWithLessEqualComparisonFilter = TRUE;

  m_d3d11Options1.ClearViewAlsoSupportsDepthOnlyFormats = TRUE;
  // Immediate-context Map only accepts dynamic or staging buffers.
  m_d3d11Options1.MapOnDefaultBuffers = FALSE;
  /**
  TODO: supported by Apple10 family
  */
  m_d3d11Options1.MinMaxFiltering = FALSE;
  m_d3d11Options1.TiledResourcesTier = D3D11_TILED_RESOURCES_NOT_SUPPORTED;

  m_d3d11Options2.TiledResourcesTier = D3D11_TILED_RESOURCES_NOT_SUPPORTED;
  m_d3d11Options2.ConservativeRasterizationTier =
      D3D11_CONSERVATIVE_RASTERIZATION_NOT_SUPPORTED;
  m_d3d11Options2.PSSpecifiedStencilRefSupported = TRUE;
  // Rasterizer-ordered views have neither a D3D11 interface implementation nor
  // AIR shader conversion support. Do not expose an unsupported feature path.
  m_d3d11Options2.ROVsSupported = FALSE;
  // Immediate-context Map only accepts dynamic or staging textures.
  m_d3d11Options2.MapOnDefaultTextures = FALSE;
  // Tiled resources are unsupported and texture views use identity swizzles.
  m_d3d11Options2.StandardSwizzle = FALSE;
  // Typed UAV loads exist, but additional-format coverage is not implemented
  // or validated against the D3D11 format requirements.
  m_d3d11Options2.TypedUAVLoadAdditionalFormats = FALSE;
  /**
  It's an intention to not report UMA
   */
  m_d3d11Options2.UnifiedMemoryArchitecture = FALSE;
}

HRESULT MTLD3D11Inspection::GetFeatureData(D3D11_FEATURE Feature,
                                           UINT FeatureDataSize,
                                           void *pFeatureData) const {
  // MSDN order
  // https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_feature
  // so far support dx11.1
  switch (Feature) {
  case D3D11_FEATURE_THREADING:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_threading);
  case D3D11_FEATURE_DOUBLES:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_doubles);
  case D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d10Options);
  case D3D11_FEATURE_D3D11_OPTIONS:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options);
  case D3D11_FEATURE_ARCHITECTURE_INFO:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData,
                               &m_architectureInfo);
  case D3D11_FEATURE_D3D9_OPTIONS:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d9Options);
  case D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData,
                               &m_shaderMinPrecision);
  case D3D11_FEATURE_D3D9_SHADOW_SUPPORT:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d9Shadow);
  case D3D11_FEATURE_D3D11_OPTIONS1:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options1);
    //   case D3D11_FEATURE_D3D9_SIMPLE_INSTANCING_SUPPORT:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData,
    //                                &m_d3d9SimpleInstancing);
    //   case D3D11_FEATURE_MARKER_SUPPORT:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_marker);
    //   case D3D11_FEATURE_D3D9_OPTIONS1:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData,
    //     &m_d3d9Options1);
  case D3D11_FEATURE_D3D11_OPTIONS2:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options2);
  case D3D11_FEATURE_D3D11_OPTIONS3:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options3);
    //   case D3D11_FEATURE_D3D11_OPTIONS4:
    //     return GetTypedFeatureData(FeatureDataSize, pFeatureData,
    //     &m_d3d11Options4);
  case D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData,
                               &m_gpuVirtualAddress);
  case D3D11_FEATURE_D3D11_OPTIONS5:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_d3d11Options5);
  case D3D11_FEATURE_SHADER_CACHE:
    return GetTypedFeatureData(FeatureDataSize, pFeatureData, &m_shaderCache);
  default:
    ERR("Not supported feature: ", Feature);
    return E_INVALIDARG;
  }
}

} // namespace dxmt