// HUD write-back cost harness: the tail of a flat station frame at UI quality 100, in the order a draw census recorded it
// (EDVR flight 2026-10-28 16:20, frames q 1244..1376), drawn with stand-in shaders, so the GPU cost of the depth-stencil
// write-back can be compared across the ways it can be issued.
// dx11_hud_wb <mode> <seconds> [scene iterations] [holo coverage %] [frames]
//   mode 0: each stencil-writing HUD draw is re-issued into the game's depth-stencil with its own pixel shader and every
//           colour write masked off (EDVR today); 1: the same re-issue with no pixel shader bound; 2: no re-issue;
//   3: no HUD at all (the scene and the composite only); for attribution only, 4: the re-issue with its colour written
//   (changes the picture), 5: mode 0 without the second seed, 6: mode 0 without the tonemap re-issue,
//   7: mode 0 plus the frame's dead clears -- the 13 panel targets (UI 100 sizes) and their depth-stencils the census saw
//   cleared every frame and never drawn, sampled or copied, and the radar target's two superseded clears.
//   seconds 0 runs unpaced for <frames> timed frames after a warm-up and prints the mean frame time (GPU-bound); otherwise
//   the frame is paced to 60 Hz for that long, printing frames per second every five seconds and DONE at the end, so an
//   external sampler can read GPU power.
// Each frame: a scene pass at 1752x1132 (R11G11B10 colour, D32S8 reversed depth with stencil), then the HUD tail: a
// full-size layer depth seed, 30 flight-HUD quads into a 2336x1510 RGBA16F layer testing the seeded depth, 25 game draws,
// the target sprite (a second seed, its draw and its write-back), 12 holo panels (a heavy material, stencil 0x04 REPLACE,
// each followed by its write-back) interleaved with 16 stencil-writing game draws, the tonemap re-issue of the layer into
// an 8-bit layer, and a compute composite. Prints a hash of the game's depth-stencil and the composite, read back once
// after the warm-up: modes 0 and 1 must match bit for bit.
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void check(HRESULT hr, const char *what) {
  if (FAILED(hr)) { std::printf("FAIL %s 0x%08lx\n", what, (unsigned long)hr); std::exit(1); }
}
static ID3DBlob *compile(const char *src, const char *entry, const char *target) {
  ID3DBlob *b = nullptr, *e = nullptr;
  if (FAILED(D3DCompile(src, std::strlen(src), nullptr, nullptr, nullptr, entry, target, 0, 0, &b, &e))) {
    std::printf("FAIL compile %s: %s\n", entry, e ? (const char *)e->GetBufferPointer() : "?"); std::exit(1);
  }
  return b;
}

static const char *kShaders = R"(
cbuffer D : register(b0) { float4 rect; float4 tint; float4 misc; };   // rect: NDC x0,y0,x1,y1; misc.x depth, misc.y loop
struct V { float4 p : SV_POSITION; float2 uv : UV; };
V quad(uint i : SV_VertexID) {
  uint c = i % 6; float2 t = float2(c == 1 || c == 4 || c == 5 ? 1 : 0, c == 2 || c == 3 || c == 5 ? 1 : 0);
  V v; v.uv = t; v.p = float4(lerp(rect.xy, rect.zw, t), misc.x, 1); return v;
}
float4 scene(V v) : SV_Target {
  float3 c = float3(v.uv, tint.x);
  for (int i = 0; i < (int)misc.y; i++) c = frac(c * 1.7319 + c.yzx * 0.3171 + 0.123);
  return float4(c, 1);
}
Texture2D<float4> T0 : register(t0); Texture2D<float4> T1 : register(t1); Texture2D<float4> T2 : register(t2);
SamplerState S0 : register(s0);
float4 flat(V v) : SV_Target { float4 s = T0.Sample(S0, v.uv); return float4(s.rgb * tint.rgb * s.a, s.a * tint.a); }
// The holo panel material, approximated: an eight-tap smear of the interface surface, a mask, and a four-moment
// shadow gather with its arithmetic -- the lit material holo_material.h describes.
float4 holo(V v) : SV_Target {
  float4 acc = 0;
  [unroll] for (int k = 0; k < 8; k++) acc += T0.Sample(S0, v.uv + float2(k * 0.0021, k * 0.0007));
  float mask = T1.Sample(S0, v.uv * 0.5).r;
  float4 m = T2.Gather(S0, v.uv * 0.75 + 0.1);
  float b0 = m.x, b1 = m.y, b2 = m.z, b3 = m.w;
  float d = saturate(v.uv.x * 0.9 + 0.05), d2 = d * d;
  float L32 = b1 - b0 * b0 + 1e-4, L21 = (b2 - b0 * b1) / L32, L33 = b3 - b1 * b1 - L21 * L21 * L32 + 1e-4;
  float c1 = d - b0, c2 = d2 - b1 - L21 * c1, c3 = c2 / L33;
  float sh = saturate(1 - (c1 * c1 / L32 + c3 * c3 * 0.25));
  float4 col = acc * 0.125 * mask * (0.4 + 0.6 * sh) * tint;
  return float4(col.rgb * col.a, col.a);
}
Texture2D<float> GD : register(t3); Texture2D<uint2> GS : register(t4);
cbuffer Seed : register(b1) { uint2 inSize; uint2 outSize; };
struct SO { float depth : SV_Depth; uint stencil : SV_StencilRef; };
SO seed(float4 p : SV_Position) {
  int2 q = clamp(int2(floor(p.xy * float2(inSize) / float2(outSize))), int2(0, 0), int2(inSize) - 1);
  SO o; o.depth = GD.Load(int3(q, 0)); o.stencil = GS.Load(int3(q, 0)).g; return o;
}
float4 tone(V v) : SV_Target { float4 h = T0.Load(int3(v.p.xy, 0)); float3 c = h.rgb / (1 + h.rgb); c = pow(saturate(c), 1 / 2.2); return float4(c, h.a); }
Texture2D<float4> base : register(t0); Texture2D<float4> layer : register(t1); RWTexture2D<float4> outp : register(u0);
[numthreads(8, 8, 1)] void composite(uint3 id : SV_DispatchThreadID) { float4 l = layer[id.xy]; outp[id.xy] = float4(base[id.xy / 1.3333].rgb * (1 - l.a) + l.rgb, 1); }
)";

struct DrawConsts { float rect[4]; float tint[4]; float misc[4]; };

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const int mode = argc > 1 ? std::atoi(argv[1]) : 0;
  const double seconds = argc > 2 ? std::atof(argv[2]) : 0;
  const int sceneIters = argc > 3 ? std::atoi(argv[3]) : 24;
  const float cover = (argc > 4 ? (float)std::atof(argv[4]) : 15.f) / 100.f;
  const int timedFrames = argc > 5 ? std::atoi(argv[5]) : 600;
  const UINT GW = 1752, GH = 1132, LW = 2336, LH = 1510;

  ID3D11Device *dev; ID3D11DeviceContext *ctx;
  check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx), "device");
  auto tex = [&](UINT w, UINT h, DXGI_FORMAT f, UINT bind, const void *init = nullptr, UINT pitch = 0) {
    D3D11_TEXTURE2D_DESC d = {}; d.Width = w; d.Height = h; d.MipLevels = d.ArraySize = 1; d.Format = f; d.SampleDesc.Count = 1; d.BindFlags = bind;
    D3D11_SUBRESOURCE_DATA s = {init, pitch, 0};
    ID3D11Texture2D *t; check(dev->CreateTexture2D(&d, init ? &s : nullptr, &t), "texture"); return t;
  };
  const UINT rtSrv = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  auto gameRt = tex(GW, GH, DXGI_FORMAT_R11G11B10_FLOAT, rtSrv);
  auto gameDs = tex(GW, GH, DXGI_FORMAT_R32G8X24_TYPELESS, D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
  auto layerRt = tex(LW, LH, DXGI_FORMAT_R16G16B16A16_FLOAT, rtSrv);
  auto layerDs = tex(LW, LH, DXGI_FORMAT_R32G8X24_TYPELESS, D3D11_BIND_DEPTH_STENCIL);
  auto toneRt = tex(LW, LH, DXGI_FORMAT_R8G8B8A8_UNORM, rtSrv);
  auto out = tex(LW, LH, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_UNORDERED_ACCESS);
  // Inputs: the interface surface, the mask, the shadow atlas, the HUD sprite sheet.
  static uint32_t surf[1024 * 512], small[256 * 256];
  for (UINT i = 0; i < 1024 * 512; i++) surf[i] = 0x80000000u | ((i * 2654435761u) & 0x00FFFFFFu);
  for (UINT i = 0; i < 256 * 256; i++) small[i] = (i & 7) ? 0xC0A0F0B0u : 0x00000000u;
  static uint16_t atlas[1024 * 512 * 4];
  for (UINT i = 0; i < 1024 * 512 * 4; i++) atlas[i] = (uint16_t)(0x3400 + (i * 37) % 0x0800);
  auto surfTex = tex(1024, 512, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, surf, 1024 * 4);
  auto maskTex = tex(256, 256, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, small, 256 * 4);
  auto atlasTex = tex(1024, 512, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE, atlas, 1024 * 8);

  ID3D11RenderTargetView *gameRtv, *layerRtv, *toneRtv;
  ID3D11DepthStencilView *gameDsv, *layerDsv;
  ID3D11ShaderResourceView *gameColorSrv, *gameDepthSrv, *gameStencilSrv, *layerSrv, *toneSrv, *surfSrv, *maskSrv, *atlasSrv;
  ID3D11UnorderedAccessView *outUav;
  check(dev->CreateRenderTargetView(gameRt, nullptr, &gameRtv), "game rtv");
  check(dev->CreateRenderTargetView(layerRt, nullptr, &layerRtv), "layer rtv");
  check(dev->CreateRenderTargetView(toneRt, nullptr, &toneRtv), "tone rtv");
  D3D11_DEPTH_STENCIL_VIEW_DESC dv = {}; dv.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT; dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
  check(dev->CreateDepthStencilView(gameDs, &dv, &gameDsv), "game dsv");
  check(dev->CreateDepthStencilView(layerDs, &dv, &layerDsv), "layer dsv");
  D3D11_SHADER_RESOURCE_VIEW_DESC sd = {}; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
  sd.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; check(dev->CreateShaderResourceView(gameDs, &sd, &gameDepthSrv), "depth srv");
  sd.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT; check(dev->CreateShaderResourceView(gameDs, &sd, &gameStencilSrv), "stencil srv");
  check(dev->CreateShaderResourceView(gameRt, nullptr, &gameColorSrv), "game srv");
  check(dev->CreateShaderResourceView(layerRt, nullptr, &layerSrv), "layer srv");
  check(dev->CreateShaderResourceView(toneRt, nullptr, &toneSrv), "tone srv");
  check(dev->CreateShaderResourceView(surfTex, nullptr, &surfSrv), "surf srv");
  check(dev->CreateShaderResourceView(maskTex, nullptr, &maskSrv), "mask srv");
  check(dev->CreateShaderResourceView(atlasTex, nullptr, &atlasSrv), "atlas srv");
  check(dev->CreateUnorderedAccessView(out, nullptr, &outUav), "out uav");

  auto bvs = compile(kShaders, "quad", "vs_5_0");
  ID3D11VertexShader *vs; check(dev->CreateVertexShader(bvs->GetBufferPointer(), bvs->GetBufferSize(), nullptr, &vs), "vs");
  auto ps = [&](const char *entry) {
    auto b = compile(kShaders, entry, "ps_5_0"); ID3D11PixelShader *p;
    check(dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &p), entry); return p;
  };
  ID3D11PixelShader *psScene = ps("scene"), *psFlat = ps("flat"), *psHolo = ps("holo"), *psSeed = ps("seed"), *psTone = ps("tone");
  auto bcs = compile(kShaders, "composite", "cs_5_0");
  ID3D11ComputeShader *cs; check(dev->CreateComputeShader(bcs->GetBufferPointer(), bcs->GetBufferSize(), nullptr, &cs), "cs");

  D3D11_BUFFER_DESC bd = {}; bd.ByteWidth = sizeof(DrawConsts); bd.Usage = D3D11_USAGE_DYNAMIC; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  ID3D11Buffer *cb; check(dev->CreateBuffer(&bd, nullptr, &cb), "cb");
  UINT seedSize[4] = {GW, GH, LW, LH}; D3D11_SUBRESOURCE_DATA si = {seedSize, 0, 0};
  bd.ByteWidth = 16; bd.Usage = D3D11_USAGE_IMMUTABLE; bd.CPUAccessFlags = 0;
  ID3D11Buffer *cbSeed; check(dev->CreateBuffer(&bd, &si, &cbSeed), "cb seed");

  auto dss = [&](BOOL depth, D3D11_DEPTH_WRITE_MASK write, D3D11_COMPARISON_FUNC func, BOOL stencil, UINT8 wmask,
                 D3D11_COMPARISON_FUNC sfunc, UINT8 rmask) {
    D3D11_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = depth; d.DepthWriteMask = write; d.DepthFunc = func;
    d.StencilEnable = stencil; d.StencilReadMask = rmask; d.StencilWriteMask = wmask;
    d.FrontFace = {D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_REPLACE, sfunc}; d.BackFace = d.FrontFace;
    ID3D11DepthStencilState *s; check(dev->CreateDepthStencilState(&d, &s), "dss"); return s;
  };
  auto dsScene = dss(TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_GREATER_EQUAL, TRUE, 0x15, D3D11_COMPARISON_ALWAYS, 0);
  auto dsSeed = dss(TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_ALWAYS, TRUE, 0xFF, D3D11_COMPARISON_ALWAYS, 0xFF);
  auto dsHud = dss(TRUE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_GREATER_EQUAL, FALSE, 0, D3D11_COMPARISON_ALWAYS, 0);
  auto dsHolo = dss(TRUE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_GREATER_EQUAL, TRUE, 0x04, D3D11_COMPARISON_ALWAYS, 0);
  auto dsSprite = dss(FALSE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_ALWAYS, TRUE, 0x05, D3D11_COMPARISON_EQUAL, 0x01);
  auto dsGameStencil = dss(TRUE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_GREATER_EQUAL, TRUE, 0x04, D3D11_COMPARISON_ALWAYS, 0);
  auto dsGameDepth = dss(TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_GREATER_EQUAL, TRUE, 0x15, D3D11_COMPARISON_ALWAYS, 0);
  auto dsOff = dss(FALSE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_ALWAYS, FALSE, 0, D3D11_COMPARISON_ALWAYS, 0);
  auto blend = [&](BOOL on, D3D11_BLEND src, D3D11_BLEND dst, UINT8 mask) {
    D3D11_BLEND_DESC d = {}; auto &r = d.RenderTarget[0];
    r.BlendEnable = on; r.SrcBlend = r.SrcBlendAlpha = src; r.DestBlend = r.DestBlendAlpha = dst; r.BlendOp = r.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    r.RenderTargetWriteMask = mask; ID3D11BlendState *s; check(dev->CreateBlendState(&d, &s), "blend"); return s;
  };
  auto bOpaque = blend(FALSE, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, 15), bPremul = blend(TRUE, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, 15);
  auto bPremulRgb = blend(TRUE, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, 7), bNone = blend(FALSE, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, 0);
  D3D11_SAMPLER_DESC smp = {}; smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
  smp.MaxLOD = D3D11_FLOAT32_MAX; ID3D11SamplerState *sampler; check(dev->CreateSamplerState(&smp, &sampler), "sampler");
  D3D11_RASTERIZER_DESC rd = {}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
  ID3D11RasterizerState *rs; check(dev->CreateRasterizerState(&rd, &rs), "rs");

  auto consts = [&](float x0, float y0, float x1, float y1, float depth, float loop, float r, float g, float b, float a) {
    D3D11_MAPPED_SUBRESOURCE m; check(ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), "map");
    DrawConsts c = {{x0, y0, x1, y1}, {r, g, b, a}, {depth, loop, 0, 0}}; std::memcpy(m.pData, &c, sizeof c); ctx->Unmap(cb, 0);
  };
  auto viewport = [&](UINT w, UINT h) { D3D11_VIEWPORT v = {0, 0, (float)w, (float)h, 0, 1}; ctx->RSSetViewports(1, &v); };
  auto bindGame = [&]() { ctx->OMSetRenderTargets(1, &gameRtv, gameDsv); viewport(GW, GH); };
  auto bindLayer = [&](bool dsv) { ctx->OMSetRenderTargets(1, &layerRtv, dsv ? layerDsv : nullptr); viewport(LW, LH); };
  // A pseudo-random rectangle of the given area fraction, in NDC.
  unsigned rng = 12345;
  auto rnd = [&]() { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.f; };
  struct Rect { float x0, y0, x1, y1; };
  auto rectOf = [&](float area, float aspect) {
    float w = 2 * std::sqrt(area * aspect), h = 2 * std::sqrt(area / aspect); if (w > 1.9f) w = 1.9f; if (h > 1.9f) h = 1.9f;
    float x = -1 + rnd() * (2 - w), y = -1 + rnd() * (2 - h); return Rect{x, y, x + w, y + h};
  };
  Rect hudRects[30], gameRects[25], holoRects[12], panelGameRects[16], sprite;
  for (auto &r : hudRects) r = rectOf(0.004f, 3.f);
  for (auto &r : gameRects) r = rectOf(0.01f, 1.f);
  for (auto &r : holoRects) r = rectOf(cover / 12, 1.6f);
  for (int i = 0; i < 16; i++) { Rect h = holoRects[i % 12]; panelGameRects[i] = {h.x0, h.y0, h.x0 + (h.x1 - h.x0) * 0.3f, h.y0 + (h.y1 - h.y0) * 0.3f}; }
  sprite = rectOf(0.002f, 1.f);

  auto seed = [&]() {
    ctx->OMSetRenderTargets(1, &layerRtv, layerDsv); viewport(LW, LH);   // EDVR's seed keeps the layer colour bound, masked
    ctx->OMSetBlendState(bNone, nullptr, ~0u); ctx->OMSetDepthStencilState(dsSeed, 0);
    consts(-1, -1, 1, 1, 0, 0, 0, 0, 0, 0);
    ctx->PSSetShader(psSeed, nullptr, 0); ID3D11ShaderResourceView *s[2] = {gameDepthSrv, gameStencilSrv}; ctx->PSSetShaderResources(3, 2, s);
    ctx->PSSetConstantBuffers(1, 1, &cbSeed); ctx->Draw(6, 0);
    ID3D11ShaderResourceView *n[2] = {}; ctx->PSSetShaderResources(3, 2, n);
  };
  // A redirected HUD draw into the layer, then (when it writes stencil, modes 0 and 1) its write-back into the game's buffer.
  auto hudDraw = [&](const Rect &r, ID3D11PixelShader *p, ID3D11DepthStencilState *ds, UINT ref, bool writes, float depth) {
    bindLayer(true); ctx->OMSetBlendState(bPremulRgb, nullptr, ~0u); ctx->OMSetDepthStencilState(ds, ref);
    consts(r.x0, r.y0, r.x1, r.y1, depth, 0, 0.6f, 0.8f, 1.0f, 0.7f); ctx->PSSetShader(p, nullptr, 0); ctx->Draw(6, 0);
    if (writes && (mode <= 1 || mode >= 4)) {
      bindGame(); ctx->OMSetBlendState(mode == 4 ? bPremul : bNone, nullptr, ~0u); ctx->OMSetDepthStencilState(ds, ref);
      ctx->PSSetShader(mode == 1 ? nullptr : p, nullptr, 0); ctx->Draw(6, 0);
    }
  };
  auto gameDraw = [&](const Rect &r, ID3D11DepthStencilState *ds, UINT ref, float depth) {
    bindGame(); ctx->OMSetBlendState(bPremul, nullptr, ~0u); ctx->OMSetDepthStencilState(ds, ref);
    consts(r.x0, r.y0, r.x1, r.y1, depth, 0, 0.3f, 0.3f, 0.4f, 0.5f); ctx->PSSetShader(psFlat, nullptr, 0); ctx->Draw(6, 0);
  };

  // The dead clears of the station frame (census 2026-10-28 16:22, q 51..63, 768, 1204..1216).
  struct Dead { UINT w, h; bool ds; };
  const Dead dead[] = {{867, 253, false}, {867, 253, true}, {1051, 262, false}, {1661, 1661, false}, {1868, 1070, false}, {1868, 1070, true},
                       {1868, 1070, false}, {1868, 1070, true}, {1713, 1070, false}, {1713, 1070, true}, {1051, 262, false}, {1297, 1297, false},
                       {1297, 1297, true}, {1024, 512, false}};
  ID3D11RenderTargetView *deadRtv[16] = {}; ID3D11DepthStencilView *deadDsv[16] = {};
  const unsigned nDead = sizeof(dead) / sizeof(dead[0]);
  for (unsigned i = 0; i < nDead; i++) {
    if (dead[i].ds) {
      auto t = tex(dead[i].w, dead[i].h, DXGI_FORMAT_R24G8_TYPELESS, D3D11_BIND_DEPTH_STENCIL);
      D3D11_DEPTH_STENCIL_VIEW_DESC v = {}; v.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; v.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
      check(dev->CreateDepthStencilView(t, &v, &deadDsv[i]), "dead dsv");
    } else {
      auto t = tex(dead[i].w, dead[i].h, DXGI_FORMAT_R8G8B8A8_UNORM, rtSrv); check(dev->CreateRenderTargetView(t, nullptr, &deadRtv[i]), "dead rtv");
    }
  }
  auto radarRt = tex(1024, 1024, DXGI_FORMAT_R11G11B10_FLOAT, rtSrv); auto radarDs = tex(1024, 1024, DXGI_FORMAT_R16_TYPELESS, D3D11_BIND_DEPTH_STENCIL);
  ID3D11RenderTargetView *radarRtv; ID3D11DepthStencilView *radarDsv; check(dev->CreateRenderTargetView(radarRt, nullptr, &radarRtv), "radar rtv");
  { D3D11_DEPTH_STENCIL_VIEW_DESC v = {}; v.Format = DXGI_FORMAT_D16_UNORM; v.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D; check(dev->CreateDepthStencilView(radarDs, &v, &radarDsv), "radar dsv"); }

  auto frame = [&](long n) {
    if (mode == 7) {
      float z[4] = {0, 0, 0, 0};
      for (unsigned i = 0; i < nDead; i++) {
        if (deadDsv[i]) ctx->ClearDepthStencilView(deadDsv[i], D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0, 0); else ctx->ClearRenderTargetView(deadRtv[i], z);
      }
    }
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); ctx->IASetInputLayout(nullptr); ctx->RSSetState(rs);
    ctx->VSSetShader(vs, nullptr, 0); ctx->VSSetConstantBuffers(0, 1, &cb); ctx->PSSetConstantBuffers(0, 1, &cb);
    ID3D11ShaderResourceView *in[3] = {surfSrv, maskSrv, atlasSrv}; ctx->PSSetShaderResources(0, 3, in); ctx->PSSetSamplers(0, 1, &sampler);
    float black[4] = {0, 0, 0, 1}, clear[4] = {0, 0, 0, 0};
    ctx->ClearRenderTargetView(gameRtv, black); ctx->ClearDepthStencilView(gameDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0, 0);
    bindGame(); ctx->OMSetBlendState(bOpaque, nullptr, ~0u); ctx->OMSetDepthStencilState(dsScene, 0x14);
    consts(-1, -1, 1, 1, 0.5f, (float)sceneIters, (n % 97) / 97.f, 0, 0, 1); ctx->PSSetShader(psScene, nullptr, 0); ctx->Draw(6, 0);
    if (mode != 3) {
      ctx->ClearRenderTargetView(layerRtv, clear);
      seed();
      for (int i = 0; i < 30; i++) hudDraw(hudRects[i], psFlat, dsHud, 0, false, 0.9f);
      for (int i = 0; i < 25; i++) gameDraw(gameRects[i], i < 3 ? dsGameStencil : dsGameDepth, 0x14, 0.6f);
      // The radar target: cleared three times, drawn after the last (two superseded clears in mode 7).
      { float z[4] = {0, 0, 0, 0};
        for (int c = mode == 7 ? 0 : 2; c < 3; c++) { ctx->ClearRenderTargetView(radarRtv, z); ctx->ClearDepthStencilView(radarDsv, D3D11_CLEAR_DEPTH, 0, 0); }
        ctx->OMSetRenderTargets(1, &radarRtv, radarDsv); viewport(1024, 1024); ctx->OMSetBlendState(bPremul, nullptr, ~0u); ctx->OMSetDepthStencilState(dsOff, 0);
        consts(-0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0, 0.2f, 0.4f, 0.6f, 0.5f); ctx->PSSetShader(psFlat, nullptr, 0); ctx->Draw(6, 0); }
      if (mode != 5) seed();   // the game's depth-and-stencil writers made the first seed stale
      hudDraw(sprite, psFlat, dsSprite, 0x15, true, 0.9f);
      for (int i = 0, g = 0; i < 12; i++) {
        hudDraw(holoRects[i], psHolo, dsHolo, 0x0E, true, 0.9f);
        for (int k = 0; k < (i < 4 ? 2 : 1) && g < 16; k++, g++) gameDraw(panelGameRects[g], dsGameStencil, 0x0E, 0.9f);
      }
      // The tonemap re-issue into the 8-bit layer, reading the HDR layer.
      if (mode != 6) ctx->OMSetRenderTargets(1, &toneRtv, nullptr); viewport(LW, LH); ctx->OMSetBlendState(bOpaque, nullptr, ~0u); ctx->OMSetDepthStencilState(dsOff, 0);
      consts(-1, -1, 1, 1, 0, 0, 0, 0, 0, 0); ctx->PSSetShaderResources(0, 1, &layerSrv); ctx->PSSetShader(psTone, nullptr, 0);
      if (mode != 6) ctx->Draw(6, 0);
    }
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView *none[3] = {}; ctx->PSSetShaderResources(0, 3, none);
    ID3D11ShaderResourceView *c[2] = {gameColorSrv, toneSrv}; ctx->CSSetShaderResources(0, 2, c); ctx->CSSetUnorderedAccessViews(0, 1, &outUav, nullptr);
    ctx->CSSetShader(cs, nullptr, 0); ctx->Dispatch((LW + 7) / 8, (LH + 7) / 8, 1);
    ID3D11UnorderedAccessView *nu = nullptr; ctx->CSSetShaderResources(0, 2, none); ctx->CSSetUnorderedAccessViews(0, 1, &nu, nullptr);
  };

  // The game's depth-stencil after one frame, hashed (stencil and depth), and the composite: modes 0 and 1 must agree.
  auto hashOf = [&](ID3D11Texture2D *t, UINT bytesPerPixel) {
    D3D11_TEXTURE2D_DESC d; t->GetDesc(&d); d.BindFlags = 0; d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *s; check(dev->CreateTexture2D(&d, nullptr, &s), "staging"); ctx->CopyResource(s, t);
    D3D11_MAPPED_SUBRESOURCE m; check(ctx->Map(s, 0, D3D11_MAP_READ, 0, &m), "map read");
    uint64_t h = 1469598103934665603ull;
    for (UINT y = 0; y < d.Height; y++) { auto row = (const uint8_t *)m.pData + (size_t)y * m.RowPitch; for (UINT b = 0; b < d.Width * bytesPerPixel; b++) { h ^= row[b]; h *= 1099511628211ull; } }
    ctx->Unmap(s, 0); s->Release(); return h;
  };
  frame(0);
  // The stencil the frame left in the game's buffer, made visible: a full-screen pass per bit into an R8 target, writing
  // where the bit is set (a depth-stencil readback need not carry the stencil plane).
  auto probe = tex(GW, GH, DXGI_FORMAT_R8_UNORM, D3D11_BIND_RENDER_TARGET);
  ID3D11RenderTargetView *probeRtv; check(dev->CreateRenderTargetView(probe, nullptr, &probeRtv), "probe rtv");
  auto stencilBit = [&](UINT8 bit) {
    float zero[4] = {}; ctx->ClearRenderTargetView(probeRtv, zero);
    auto ds = dss(FALSE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_ALWAYS, TRUE, 0, D3D11_COMPARISON_EQUAL, bit);
    ctx->OMSetRenderTargets(1, &probeRtv, gameDsv); viewport(GW, GH); ctx->OMSetBlendState(bOpaque, nullptr, ~0u); ctx->OMSetDepthStencilState(ds, bit);
    ID3D11ShaderResourceView *in[3] = {surfSrv, maskSrv, atlasSrv}; ctx->PSSetShaderResources(0, 3, in);
    consts(-1, -1, 1, 1, 0.5f, 0, 1, 1, 1, 1); ctx->PSSetShader(psScene, nullptr, 0); ctx->Draw(6, 0);
    ctx->OMSetRenderTargets(0, nullptr, nullptr); ds->Release();
    D3D11_TEXTURE2D_DESC d; probe->GetDesc(&d); d.BindFlags = 0; d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *s; check(dev->CreateTexture2D(&d, nullptr, &s), "probe staging"); ctx->CopyResource(s, probe);
    D3D11_MAPPED_SUBRESOURCE m; check(ctx->Map(s, 0, D3D11_MAP_READ, 0, &m), "probe map");
    unsigned n = 0; for (UINT y = 0; y < GH; y++) for (UINT x = 0; x < GW; x++) n += ((const uint8_t *)m.pData)[(size_t)y * m.RowPitch + x] != 0;
    ctx->Unmap(s, 0); s->Release(); return n;
  };
  const unsigned bit4 = stencilBit(0x04), bit1 = stencilBit(0x01);
  frame(0);
  std::printf("RUN mode=%d seconds=%.0f scene_iters=%d holo_cover=%.0f%% stencil 0x04 pixels=%u 0x01 pixels=%u composite=%016llx\n", mode,
              seconds, sceneIters, cover * 100, bit4, bit1, (unsigned long long)hashOf(out, 8));

  D3D11_QUERY_DESC qd = {D3D11_QUERY_EVENT, 0}; ID3D11Query *fence[3]; for (auto &f : fence) check(dev->CreateQuery(&qd, &f), "event");
  LARGE_INTEGER freq, start, now; QueryPerformanceFrequency(&freq);
  auto clock = [&]() { QueryPerformanceCounter(&now); return double(now.QuadPart - start.QuadPart) / freq.QuadPart; };
  if (seconds <= 0) {
    // Unpaced: two frames in flight, the GPU the bottleneck; the mean over the timed frames is GPU time a frame.
    long total = 120 + timedFrames; double t0 = 0;
    QueryPerformanceCounter(&start);
    for (long f = 0; f < total; f++) {
      if (f == 120) t0 = clock();
      if (f >= 2) while (ctx->GetData(fence[(f - 2) % 3], nullptr, 0, 0) == S_FALSE) Sleep(0);
      frame(f); ctx->End(fence[f % 3]); ctx->Flush();
    }
    for (long f = total - 2; f < total; f++) while (ctx->GetData(fence[f % 3], nullptr, 0, 0) == S_FALSE) Sleep(0);
    std::printf("TIMED mode=%d ms/frame=%.4f frames=%d\n", mode, (clock() - t0) * 1000 / timedFrames, timedFrames);
  } else {
    QueryPerformanceCounter(&start); long f = 0, reportFrames = 0; double lastReport = 0;
    for (;;) {
      double t = clock(); if (t >= seconds) break;
      if (f >= 2) while (ctx->GetData(fence[(f - 2) % 3], nullptr, 0, 0) == S_FALSE) Sleep(0);
      frame(f); ctx->End(fence[f % 3]); ctx->Flush(); ++f; ++reportFrames;
      double next = f / 60.0; for (;;) { double tn = clock(); if (tn >= next) break; if (next - tn > 0.002) Sleep(1); }
      if (t - lastReport >= 5) { std::printf("t=%.0f frames/s=%.1f\n", t, reportFrames / (t - lastReport)); lastReport = t; reportFrames = 0; }
    }
    std::printf("DONE frames=%ld\n", f);
  }
  check(dev->GetDeviceRemovedReason(), "device status");
  return 0;
}
