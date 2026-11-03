// Deferred clears: every way a texture can see a clear made before it, checked by the pixels that come out.
// dx11_deferred_clear  -- prints PASS/FAIL per case and a summary; exit code 0 when every case passes.
// The same program runs against DXMT with d3d11.deferClears on, with DXMT_DEFER_CLEARS=0, and against a build
// without the deferral: the pixels must be the same in all three.
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <algorithm>
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
cbuffer C : register(b0) { float4 color; float4 rect; float depth; };
float4 vs(uint i : SV_VertexID) : SV_POSITION {
  uint c = i % 6; float2 t = float2(c == 1 || c == 4 || c == 5 ? 1 : 0, c == 2 || c == 3 || c == 5 ? 1 : 0);
  return float4(lerp(rect.xy, rect.zw, t), depth, 1);
}
float4 solid(float4 p : SV_POSITION) : SV_Target { return color; }
Texture2D<float4> T : register(t0);
float4 copy(float4 p : SV_POSITION) : SV_Target { return T.Load(int3(p.xy, 0)); }
RWTexture2D<float4> O : register(u0);
[numthreads(8, 8, 1)] void cs(uint3 id : SV_DispatchThreadID) { O[id.xy] = T.Load(int3(id.xy, 0)); }
)";

static const UINT W = 64, H = 64;
static unsigned g_pass = 0, g_fail = 0;

struct Rig {
  ID3D11Device *dev;
  ID3D11DeviceContext *ctx;
  ID3D11VertexShader *vs;
  ID3D11PixelShader *psSolid, *psCopy;
  ID3D11ComputeShader *cs;
  ID3D11Buffer *cb;
  ID3D11DepthStencilState *dsOff;
  ID3D11BlendState *bOpaque;
  ID3D11RasterizerState *rs;

  void init() {
    check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx), "device");
    auto b = compile(kShaders, "vs", "vs_5_0"); check(dev->CreateVertexShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &vs), "vs");
    b = compile(kShaders, "solid", "ps_5_0"); check(dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &psSolid), "solid");
    b = compile(kShaders, "copy", "ps_5_0"); check(dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &psCopy), "copy");
    b = compile(kShaders, "cs", "cs_5_0"); check(dev->CreateComputeShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &cs), "cs");
    D3D11_BUFFER_DESC bd = {}; bd.ByteWidth = 48; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check(dev->CreateBuffer(&bd, nullptr, &cb), "cb");
    D3D11_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = FALSE; check(dev->CreateDepthStencilState(&d, &dsOff), "ds off");
    D3D11_BLEND_DESC bl = {}; bl.RenderTarget[0].RenderTargetWriteMask = 15; check(dev->CreateBlendState(&bl, &bOpaque), "blend");
    D3D11_RASTERIZER_DESC rd = {}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    check(dev->CreateRasterizerState(&rd, &rs), "rasterizer");
  }
  ID3D11Texture2D *tex(DXGI_FORMAT f, UINT bind, UINT mips = 1, UINT w = W, UINT h = H) {
    D3D11_TEXTURE2D_DESC d = {}; d.Width = w; d.Height = h; d.MipLevels = mips; d.ArraySize = 1; d.Format = f; d.SampleDesc.Count = 1; d.BindFlags = bind;
    ID3D11Texture2D *t; check(dev->CreateTexture2D(&d, nullptr, &t), "texture"); return t;
  }
  ID3D11Texture2D *rgba(UINT extra = 0, UINT mips = 1) { return tex(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | extra, mips); }
  ID3D11RenderTargetView *rtv(ID3D11Texture2D *t, UINT mip = 0) {
    D3D11_RENDER_TARGET_VIEW_DESC d = {}; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; d.Texture2D.MipSlice = mip;
    ID3D11RenderTargetView *v; check(dev->CreateRenderTargetView(t, &d, &v), "rtv"); return v;
  }
  ID3D11ShaderResourceView *srv(ID3D11Texture2D *t) { ID3D11ShaderResourceView *v; check(dev->CreateShaderResourceView(t, nullptr, &v), "srv"); return v; }
  void constants(const float c[4], float x0, float y0, float x1, float y1, float depth = 0.5f) {
    float data[12] = {c[0], c[1], c[2], c[3], x0, y0, x1, y1, depth, 0, 0, 0}; ctx->UpdateSubresource(cb, 0, nullptr, data, 0, 0);
  }
  void pipeline(ID3D11DeviceContext *c, ID3D11PixelShader *ps) {
    c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); c->IASetInputLayout(nullptr);
    c->VSSetShader(vs, nullptr, 0); c->VSSetConstantBuffers(0, 1, &cb); c->PSSetShader(ps, nullptr, 0); c->PSSetConstantBuffers(0, 1, &cb);
    D3D11_VIEWPORT v = {0, 0, (float)W, (float)H, 0, 1}; c->RSSetViewports(1, &v);
    c->OMSetBlendState(bOpaque, nullptr, ~0u); c->OMSetDepthStencilState(dsOff, 0); c->RSSetState(rs);
  }
  // Copies `src` into `into` (W x H) through a pixel shader that loads it: the shader's view of the texture.
  void sampleInto(ID3D11DeviceContext *c, ID3D11ShaderResourceView *src, ID3D11RenderTargetView *into) {
    pipeline(c, psCopy);
    const float none[4] = {};
    if (c == ctx) constants(none, -1, -1, 1, 1);
    c->OMSetRenderTargets(1, &into, nullptr); c->PSSetShaderResources(0, 1, &src); c->Draw(6, 0);
    ID3D11ShaderResourceView *n = nullptr; c->PSSetShaderResources(0, 1, &n); c->OMSetRenderTargets(0, nullptr, nullptr);
  }
  uint32_t texel(ID3D11Texture2D *t, UINT x, UINT y, UINT mip = 0) {
    D3D11_TEXTURE2D_DESC d; t->GetDesc(&d);
    D3D11_TEXTURE2D_DESC s = {}; s.Width = std::max(1u, d.Width >> mip); s.Height = std::max(1u, d.Height >> mip); s.MipLevels = 1; s.ArraySize = 1;
    s.Format = d.Format; s.SampleDesc.Count = 1; s.Usage = D3D11_USAGE_STAGING; s.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *st; check(dev->CreateTexture2D(&s, nullptr, &st), "staging");
    ctx->CopySubresourceRegion(st, 0, 0, 0, 0, t, D3D11CalcSubresource(mip, 0, d.MipLevels), nullptr);
    D3D11_MAPPED_SUBRESOURCE m; check(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m), "map");
    uint32_t v = *(const uint32_t *)((const uint8_t *)m.pData + y * m.RowPitch + x * 4);
    ctx->Unmap(st, 0); st->Release(); return v;
  }
};

static void expect(const char *name, uint32_t got, uint32_t want) {
  const bool ok = got == want;
  std::printf("%s %-58s got %08x want %08x\n", ok ? "PASS" : "FAIL", name, got, want);
  ok ? ++g_pass : ++g_fail;
}

// RGBA8 as read back little-endian: 0xAABBGGRR.
static const float kRed[4] = {1, 0, 0, 1}, kGreen[4] = {0, 1, 0, 1}, kBlue[4] = {0, 0, 1, 1}, kYellow[4] = {1, 1, 0, 1},
                   kCyan[4] = {0, 1, 1, 1}, kMagenta[4] = {1, 0, 1, 1}, kWhite[4] = {1, 1, 1, 1}, kBlack[4] = {0, 0, 0, 1};
static const uint32_t RED = 0xff0000ff, GREEN = 0xff00ff00, BLUE = 0xffff0000, YELLOW = 0xff00ffff, CYAN = 0xffffff00,
                      MAGENTA = 0xffff00ff, WHITE = 0xffffffff, BLACK = 0xff000000;

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  Rig r; r.init();
  auto *ctx = r.ctx;
  auto out = r.rgba(); auto outRtv = r.rtv(out);

  { // A: a clear of an unbound texture, then a shader samples it.
    auto t = r.rgba(); auto v = r.rtv(t); auto s = r.srv(t);
    ctx->ClearRenderTargetView(v, kRed); r.sampleInto(ctx, s, outRtv);
    expect("A sampled after a clear", r.texel(out, 5, 5), RED);
  }
  { // B: the texture is bound as a shader resource when it is cleared.
    auto t = r.rgba(); auto v = r.rtv(t); auto s = r.srv(t);
    r.pipeline(ctx, r.psCopy); ctx->PSSetShaderResources(0, 1, &s);
    ctx->ClearRenderTargetView(v, kGreen);
    r.constants(kBlack, -1, -1, 1, 1); ctx->OMSetRenderTargets(1, &outRtv, nullptr); ctx->Draw(6, 0);
    ID3D11ShaderResourceView *n = nullptr; ctx->PSSetShaderResources(0, 1, &n); ctx->OMSetRenderTargets(0, nullptr, nullptr);
    expect("B cleared while bound as a shader resource", r.texel(out, 5, 5), GREEN);
  }
  { // C: cleared twice before any use: the second clear stands.
    auto t = r.rgba(); auto v = r.rtv(t); auto s = r.srv(t);
    ctx->ClearRenderTargetView(v, kBlue); ctx->ClearRenderTargetView(v, kYellow); r.sampleInto(ctx, s, outRtv);
    expect("C two clears, then sampled", r.texel(out, 5, 5), YELLOW);
  }
  { // D: cleared, then drawn into in part: the rest holds the clear (the pass's load action).
    auto t = r.rgba(); auto v = r.rtv(t);
    ctx->ClearRenderTargetView(v, kCyan);
    r.pipeline(ctx, r.psSolid); r.constants(kMagenta, -0.5f, -0.5f, 0.5f, 0.5f); ctx->OMSetRenderTargets(1, &v, nullptr); ctx->Draw(6, 0);
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    expect("D partial draw after a clear: outside", r.texel(t, 2, 2), CYAN);
    expect("D partial draw after a clear: inside", r.texel(t, W / 2, H / 2), MAGENTA);
  }
  { // E: depth and stencil cleared, then depth alone: both planes must hold (the second clear supersedes nothing).
    auto ds = r.tex(DXGI_FORMAT_R24G8_TYPELESS, D3D11_BIND_DEPTH_STENCIL);
    D3D11_DEPTH_STENCIL_VIEW_DESC dv = {}; dv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ID3D11DepthStencilView *dsv; check(r.dev->CreateDepthStencilView(ds, &dv, &dsv), "dsv");
    ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.25f, 7);
    ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 0.75f, 0);
    D3D11_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = TRUE; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; d.DepthFunc = D3D11_COMPARISON_EQUAL;
    d.StencilEnable = TRUE; d.StencilReadMask = 0xff; d.StencilWriteMask = 0;
    d.FrontFace = {D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL}; d.BackFace = d.FrontFace;
    ID3D11DepthStencilState *eq; check(r.dev->CreateDepthStencilState(&d, &eq), "ds eq");
    const float z[4] = {0, 0, 0, 1}; ctx->ClearRenderTargetView(outRtv, z);
    r.pipeline(ctx, r.psSolid); r.constants(kWhite, -1, -1, 1, 1, 0.75f); ctx->OMSetDepthStencilState(eq, 7);
    ctx->OMSetRenderTargets(1, &outRtv, dsv); ctx->Draw(6, 0); ctx->OMSetRenderTargets(0, nullptr, nullptr);
    expect("E depth 0.75 and stencil 7 both survive", r.texel(out, 9, 9), WHITE);
    ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.25f, 3);   // supersedes the depth-only clear below? no: issued first
    ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.75f, 7);   // supersedes the one above
    ctx->ClearRenderTargetView(outRtv, z);
    r.pipeline(ctx, r.psSolid); r.constants(kWhite, -1, -1, 1, 1, 0.75f); ctx->OMSetDepthStencilState(eq, 7);
    ctx->OMSetRenderTargets(1, &outRtv, dsv); ctx->Draw(6, 0); ctx->OMSetRenderTargets(0, nullptr, nullptr);
    expect("E a full clear supersedes a full clear", r.texel(out, 9, 9), WHITE);
  }
  { // F: two views of one texture, mip 1 then mip 0: each keeps its own clear.
    auto t = r.rgba(0, 2); auto v1 = r.rtv(t, 1), v0 = r.rtv(t, 0);
    ctx->ClearRenderTargetView(v1, kRed); ctx->ClearRenderTargetView(v0, kGreen);
    expect("F mip 0 keeps its clear", r.texel(t, 3, 3, 0), GREEN);
    expect("F mip 1 keeps its clear", r.texel(t, 3, 3, 1), RED);
  }
  { // G: cleared, then a corner updated from memory.
    auto t = r.rgba(); auto v = r.rtv(t);
    ctx->ClearRenderTargetView(v, kYellow);
    uint32_t black[16]; for (auto &p : black) p = BLACK;
    D3D11_BOX box = {0, 0, 0, 4, 4, 1}; ctx->UpdateSubresource(t, 0, &box, black, 16, 0);
    expect("G update after a clear: the updated corner", r.texel(t, 1, 1), BLACK);
    expect("G update after a clear: elsewhere", r.texel(t, 20, 20), YELLOW);
  }
  { // H: a command list samples the texture cleared on the immediate context before the list ran.
    ID3D11DeviceContext *def; check(r.dev->CreateDeferredContext(0, &def), "deferred");
    auto t = r.rgba(); auto v = r.rtv(t); auto s = r.srv(t);
    const float none[4] = {}; r.constants(none, -1, -1, 1, 1);
    r.sampleInto(def, s, outRtv);
    ID3D11CommandList *list; check(def->FinishCommandList(FALSE, &list), "finish");
    ctx->ClearRenderTargetView(v, kMagenta);
    ctx->ExecuteCommandList(list, TRUE); list->Release();
    expect("H a command list samples a pending clear", r.texel(out, 7, 7), MAGENTA);
    // I: a list that does not name the texture runs between its clear and its use.
    auto t2 = r.rgba(); auto v2 = r.rtv(t2); auto s2 = r.srv(t2);
    auto other = r.rgba(); auto otherRtv = r.rtv(other);
    r.pipeline(def, r.psSolid); def->OMSetRenderTargets(1, &otherRtv, nullptr); def->Draw(6, 0);
    check(def->FinishCommandList(FALSE, &list), "finish 2");
    ctx->ClearRenderTargetView(v2, kCyan);
    ctx->ExecuteCommandList(list, TRUE); list->Release();
    r.sampleInto(ctx, s2, outRtv);
    expect("I a list that does not use it runs in between", r.texel(out, 7, 7), CYAN);
    // J: a command list clears what the immediate context cleared first (the list's clear is last).
    auto t3 = r.rgba(); auto v3 = r.rtv(t3);
    def->ClearRenderTargetView(v3, kBlue);
    check(def->FinishCommandList(FALSE, &list), "finish 3");
    ctx->ClearRenderTargetView(v3, kRed);
    ctx->ExecuteCommandList(list, TRUE); list->Release();
    expect("J a list's clear after an immediate clear", r.texel(t3, 7, 7), BLUE);
    def->Release();
  }
  { // K: compute reads a cleared texture.
    auto t = r.rgba(); auto v = r.rtv(t); auto s = r.srv(t);
    auto u = r.tex(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);
    ID3D11UnorderedAccessView *uav; check(r.dev->CreateUnorderedAccessView(u, nullptr, &uav), "uav");
    ctx->ClearRenderTargetView(v, kGreen);
    ctx->CSSetShader(r.cs, nullptr, 0); ctx->CSSetShaderResources(0, 1, &s); ctx->CSSetUnorderedAccessViews(0, 1, &uav, nullptr); ctx->Dispatch(W / 8, H / 8, 1);
    ID3D11ShaderResourceView *n = nullptr; ID3D11UnorderedAccessView *nu = nullptr; ctx->CSSetShaderResources(0, 1, &n); ctx->CSSetUnorderedAccessViews(0, 1, &nu, nullptr);
    expect("K compute reads a cleared texture", r.texel(u, 11, 11), GREEN);
  }
  { // L: copied to another texture, which a shader then samples.
    auto t = r.rgba(); auto v = r.rtv(t); auto c = r.rgba(); auto cs = r.srv(c);
    ctx->ClearRenderTargetView(v, kBlue); ctx->CopyResource(c, t); r.sampleInto(ctx, cs, outRtv);
    expect("L copy of a cleared texture", r.texel(out, 13, 13), BLUE);
  }
  { // M: a partial ClearView after a full clear.
    auto t = r.rgba(); auto v = r.rtv(t);
    ID3D11DeviceContext1 *c1; check(ctx->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)&c1), "context1");
    ctx->ClearRenderTargetView(v, kRed); D3D11_RECT rc = {0, 0, 8, 8}; c1->ClearView(v, kGreen, &rc, 1); c1->Release();
    expect("M ClearView rect after a clear: inside", r.texel(t, 2, 2), GREEN);
    expect("M ClearView rect after a clear: outside", r.texel(t, 30, 30), RED);
  }
  { // N: bound for output through a second view of the same texture, then read.
    auto t = r.rgba(); auto v = r.rtv(t); auto v2 = r.rtv(t);
    ctx->ClearRenderTargetView(v, kYellow);
    r.pipeline(ctx, r.psSolid); r.constants(kBlack, 0.9f, 0.9f, 1, 1); ctx->OMSetRenderTargets(1, &v2, nullptr); ctx->Draw(6, 0); ctx->OMSetRenderTargets(0, nullptr, nullptr);
    expect("N drawn through another view after a clear", r.texel(t, 4, 4), YELLOW);
  }
  { // O: released with a clear pending, and a new texture after it.
    for (int i = 0; i < 300; i++) { auto t = r.rgba(); auto v = r.rtv(t); ctx->ClearRenderTargetView(v, kRed); v->Release(); t->Release(); }
    auto t = r.rgba(); auto v = r.rtv(t); auto s = r.srv(t); ctx->ClearRenderTargetView(v, kWhite); r.sampleInto(ctx, s, outRtv);
    expect("O after 300 released textures with pending clears", r.texel(out, 3, 3), WHITE);
  }
  { // P: cleared while bound as the render target, then drawn in part (no deferral: bound).
    auto t = r.rgba(); auto v = r.rtv(t);
    r.pipeline(ctx, r.psSolid); ctx->OMSetRenderTargets(1, &v, nullptr);
    ctx->ClearRenderTargetView(v, kBlue); r.constants(kRed, -1, -1, 0, 0); ctx->Draw(6, 0); ctx->OMSetRenderTargets(0, nullptr, nullptr);
    expect("P cleared while bound for output: undrawn part", r.texel(t, W - 2, 2), BLUE);
    expect("P cleared while bound for output: drawn part", r.texel(t, 2, H - 2), RED);
  }
  { // Q: a clear nothing ever uses, then another texture used normally (the frame's dead clears).
    auto dead = r.rgba(); auto dv = r.rtv(dead);
    for (int i = 0; i < 4; i++) ctx->ClearRenderTargetView(dv, i & 1 ? kRed : kGreen);
    auto t = r.rgba(); auto v = r.rtv(t); auto s = r.srv(t); ctx->ClearRenderTargetView(v, kMagenta); r.sampleInto(ctx, s, outRtv);
    expect("Q a used texture beside a never-used one", r.texel(out, 6, 6), MAGENTA);
    expect("Q the never-used one, read at last, holds its last clear", r.texel(dead, 6, 6), RED);
  }
  check(r.dev->GetDeviceRemovedReason(), "device status");
  std::printf("%s %u passed, %u failed\n", g_fail ? "FAIL" : "PASS", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
