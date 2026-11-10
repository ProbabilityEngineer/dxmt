#pragma once

#include "Metal.hpp"
#include "d3d11_device.hpp"
#include "d3d11_input_layout.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_state_object.hpp"
#include "util_hash.hpp"

struct MTL_GRAPHICS_PIPELINE_DESC {
  ManagedShader VertexShader;
  ManagedShader HullShader;
  ManagedShader DomainShader;
  ManagedShader GeometryShader;
  ManagedShader PixelShader;
  IMTLD3D11BlendState *BlendState;
  ManagedInputLayout InputLayout;
  IMTLD3D11StreamOutputLayout *SOLayout;
  UINT NumColorAttachments;
  WMTPixelFormat ColorAttachmentFormats[8];
  WMTPixelFormat DepthStencilFormat;
  WMTPrimitiveTopologyClass TopologyClass;
  bool RasterizationEnabled;
  uint8_t SampleCount;
  bool GSStripTopology;
  SM50_INDEX_BUFFER_FORMAT IndexBufferFormat;
  uint32_t SampleMask;
  uint32_t GSPassthrough;
};

struct MTL_COMPUTE_PIPELINE_DESC {
  ManagedShader ComputeShader;
};

struct MTL_COMPILED_GRAPHICS_PIPELINE {
  WMT::RenderPipelineState PipelineState;
};

struct MTL_COMPILED_COMPUTE_PIPELINE {
  WMT::ComputePipelineState PipelineState;
};

struct MTL_COMPILED_TESSELLATION_MESH_PIPELINE {
  WMT::RenderPipelineState PipelineState;
  uint32_t NumControlPointOutputElement;
  uint32_t ThreadsPerPatch;
};

namespace std {
template <> struct hash<MTL_GRAPHICS_PIPELINE_DESC> {
  size_t operator()(const MTL_GRAPHICS_PIPELINE_DESC &v) const noexcept {
    dxmt::HashState state;
    state.add((size_t)v.VertexShader);
    state.add((size_t)v.PixelShader);
    state.add((size_t)v.HullShader);
    state.add((size_t)v.DomainShader);
    state.add((size_t)v.GeometryShader);
    state.add((size_t)v.InputLayout);
    /* don't add blend */
    // state.add((size_t)v.BlendState);
    state.add((size_t)v.DepthStencilFormat);
    state.add((size_t)v.TopologyClass);
    state.add((size_t)v.IndexBufferFormat);
    state.add((size_t)v.GSStripTopology);
    state.add((size_t)v.SampleMask);
    state.add((size_t)v.GSPassthrough);
    state.add((size_t)v.SampleCount);
    state.add((size_t)v.NumColorAttachments);
    for (unsigned i = 0; i < v.NumColorAttachments; i++) {
      state.add(v.ColorAttachmentFormats[i]);
    }
    return state;
  };
};
template <> struct equal_to<MTL_GRAPHICS_PIPELINE_DESC> {
  bool operator()(const MTL_GRAPHICS_PIPELINE_DESC &x,
                  const MTL_GRAPHICS_PIPELINE_DESC &y) const {
    if (x.NumColorAttachments != y.NumColorAttachments)
      return false;
    for (unsigned i = 0; i < x.NumColorAttachments; i++) {
      if (x.ColorAttachmentFormats[i] != y.ColorAttachmentFormats[i])
        return false;
    }
    if (x.BlendState != y.BlendState) {
      D3D11_BLEND_DESC1 x_, y_;
      x.BlendState->GetDesc1(&x_);
      y.BlendState->GetDesc1(&y_);
      if (x_.IndependentBlendEnable != y_.IndependentBlendEnable)
        return false;
      if (x_.AlphaToCoverageEnable != y_.AlphaToCoverageEnable)
        return false;
      uint32_t num_blend_target =
          x_.IndependentBlendEnable ? x.NumColorAttachments : 1;
      for (unsigned i = 0; i < num_blend_target; i++) {
        auto &blend_target_x = x_.RenderTarget[i];
        auto &blend_target_y = y_.RenderTarget[i];
        if (blend_target_x.RenderTargetWriteMask !=
            blend_target_y.RenderTargetWriteMask)
          return false;
        if (blend_target_x.BlendEnable != blend_target_y.BlendEnable)
          return false;
        if (blend_target_x.LogicOpEnable != blend_target_y.LogicOpEnable)
          return false;
        if (blend_target_x.BlendEnable) {
          if (blend_target_x.BlendOp != blend_target_y.BlendOp)
            return false;
          if (blend_target_x.BlendOpAlpha != blend_target_y.BlendOpAlpha)
            return false;
          if (blend_target_x.SrcBlend != blend_target_y.SrcBlend)
            return false;
          if (blend_target_x.SrcBlendAlpha != blend_target_y.SrcBlendAlpha)
            return false;
          if (blend_target_x.DestBlend != blend_target_y.DestBlend)
            return false;
          if (blend_target_x.DestBlendAlpha != blend_target_y.DestBlendAlpha)
            return false;
        }
        if (blend_target_x.LogicOpEnable) {
          if (blend_target_x.LogicOp != blend_target_y.LogicOp)
            return false;
        }
      }
    }
    return (x.VertexShader == y.VertexShader) &&
           (x.PixelShader == y.PixelShader) && (x.HullShader == y.HullShader) &&
           (x.DomainShader == y.DomainShader) && 
           (x.GeometryShader == y.GeometryShader) &&
           (x.InputLayout == y.InputLayout) &&
           (x.DepthStencilFormat == y.DepthStencilFormat) &&
           (x.TopologyClass == y.TopologyClass) &&
           (x.RasterizationEnabled == y.RasterizationEnabled) &&
           (x.GSStripTopology == y.GSStripTopology) &&
           (x.SampleCount == y.SampleCount) &&
           (x.IndexBufferFormat == y.IndexBufferFormat) &&
           (x.SampleMask == y.SampleMask) &&
           (x.GSPassthrough == y.GSPassthrough);
  }
};
} // namespace std

namespace dxmt {

class MTLCompiledGraphicsPipeline : public ThreadpoolWork {
public:
  /**
  NOTE: the current thread is blocked if it's not ready
   */
  virtual void GetPipeline(MTL_COMPILED_GRAPHICS_PIPELINE *pGraphicsPipeline) = 0;
};

class MTLCompiledComputePipeline : public ThreadpoolWork {
public:
  virtual void GetPipeline(MTL_COMPILED_COMPUTE_PIPELINE *pComputePipeline) = 0;
};

class MTLCompiledGeometryPipeline : public ThreadpoolWork {
public:
  virtual void GetPipeline(MTL_COMPILED_GRAPHICS_PIPELINE *pGeometryPipeline) = 0;
};

class MTLCompiledTessellationMeshPipeline : public ThreadpoolWork {
public:
  virtual void GetPipeline(MTL_COMPILED_TESSELLATION_MESH_PIPELINE *pTessellationPipeline) = 0;
};

std::unique_ptr<MTLCompiledGraphicsPipeline>
CreateGraphicsPipeline(MTLD3D11Device *pDevice, MTL_GRAPHICS_PIPELINE_DESC *pDesc);

std::unique_ptr<MTLCompiledComputePipeline> CreateComputePipeline(MTLD3D11Device *pDevice, ManagedShader ComputeShader);

std::unique_ptr<MTLCompiledGeometryPipeline>
CreateGeometryPipeline(MTLD3D11Device *pDevice, MTL_GRAPHICS_PIPELINE_DESC *pDesc);

/**
 * Colour channels a render target format stores, as a D3D11 write mask (R=1 G=2 B=4 A=8).
 */
inline uint32_t
RenderTargetChannelMask(WMTPixelFormat format) {
  switch (format) {
  case WMTPixelFormatA8Unorm:
    return 0x8;
  case WMTPixelFormatR8Unorm:
  case WMTPixelFormatR8Unorm_sRGB:
  case WMTPixelFormatR8Snorm:
  case WMTPixelFormatR8Uint:
  case WMTPixelFormatR8Sint:
  case WMTPixelFormatR16Unorm:
  case WMTPixelFormatR16Snorm:
  case WMTPixelFormatR16Uint:
  case WMTPixelFormatR16Sint:
  case WMTPixelFormatR16Float:
  case WMTPixelFormatR32Uint:
  case WMTPixelFormatR32Sint:
  case WMTPixelFormatR32Float:
    return 0x1;
  case WMTPixelFormatRG8Unorm:
  case WMTPixelFormatRG8Unorm_sRGB:
  case WMTPixelFormatRG8Snorm:
  case WMTPixelFormatRG8Uint:
  case WMTPixelFormatRG8Sint:
  case WMTPixelFormatRG16Unorm:
  case WMTPixelFormatRG16Snorm:
  case WMTPixelFormatRG16Uint:
  case WMTPixelFormatRG16Sint:
  case WMTPixelFormatRG16Float:
  case WMTPixelFormatRG32Uint:
  case WMTPixelFormatRG32Sint:
  case WMTPixelFormatRG32Float:
    return 0x3;
  case WMTPixelFormatB5G6R5Unorm:
  case WMTPixelFormatRG11B10Float:
  case WMTPixelFormatRGB9E5Float:
  case WMTPixelFormatBGRX8Unorm:
  case WMTPixelFormatBGRX8Unorm_sRGB:
    return 0x7;
  default:
    return 0xF;
  }
}

/**
 * Apple GPU workaround (observed on M4 Max, macOS 27.3, every MSL version 2.4-4.1):
 * a fragment function that discards early and then writes colour outputs loses the
 * discard and its depth/stencil writes when an attachment has a partial write mask
 * (on 4-channel formats: any mask whose G and B bits differ). For such attachments the
 * mask is applied in the shader instead (framebuffer fetch + per-component select) and
 * the pipeline writes the attachment with a full mask.
 *
 * Returns 4 bits per render target (rt * 4): the D3D11 write mask to emulate, or 0.
 * Only shaders that discard, with blending, logic ops and dual-source blending off.
 */
inline uint32_t
EmulatedPartialWriteMasks(
    const MTL_SHADER_REFLECTION &ps_reflection, IMTLD3D11BlendState *blend_state, const WMTPixelFormat *formats,
    unsigned num_rtvs
) {
  if (!ps_reflection.PixelShader.HasDiscard || !blend_state || blend_state->IsDualSourceBlending())
    return 0;
  D3D11_BLEND_DESC1 desc;
  blend_state->GetDesc1(&desc);
  uint32_t masks = 0;
  for (unsigned rt = 0; rt < num_rtvs && rt < 8; rt++) {
    if (!(ps_reflection.PixelShader.ValidRenderTargets & (1u << rt)) || formats[rt] == WMTPixelFormatInvalid)
      continue;
    auto &target = desc.RenderTarget[desc.IndependentBlendEnable ? rt : 0];
    if (target.BlendEnable || target.LogicOpEnable)
      continue;
    uint32_t channels = RenderTargetChannelMask(formats[rt]);
    uint32_t mask = target.RenderTargetWriteMask & channels;
    if (mask == 0 || mask == channels)
      continue;
    masks |= uint32_t(target.RenderTargetWriteMask & 0xF) << (rt * 4);
  }
  return masks;
}

std::unique_ptr<MTLCompiledTessellationMeshPipeline>
CreateTessellationMeshPipeline(MTLD3D11Device *pDevice, MTL_GRAPHICS_PIPELINE_DESC *pDesc);

}; // namespace dxmt