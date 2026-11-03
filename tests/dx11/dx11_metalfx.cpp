#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include "d3d11_interfaces.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <vector>

static void check(HRESULT hr, const char *what) {
  if (FAILED(hr)) { std::printf("FAIL %s 0x%08lx\n", what, (unsigned long)hr); std::exit(1); }
}
static float half(uint16_t h) {
  return std::ldexp(float((h & 1023) + ((h & 0x7c00) ? 1024 : 0)), int((h >> 10) & 31) - 25) * ((h & 0x8000) ? -1 : 1);
}
int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  ID3D11Device *dev; ID3D11DeviceContext *ctx;
  check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,nullptr,&ctx),"device");
  for (const wchar_t *name : {L"d3d11.dll", L"dxgi.dll", L"winemetal.dll"}) {
    wchar_t path[1024] = {}; GetModuleFileNameW(GetModuleHandleW(name),path,1024);
    std::printf("MODULE %ls\n",path);
  }
  IMTLD3D11ContextExt3 *ext;
  check(ctx->QueryInterface(__uuidof(IMTLD3D11ContextExt3),(void**)&ext),"Ext3 status interface");
  const GUID old_ext2 = {0x6523d273,0x9915,0x44b3,{0xa5,0xf0,0xf6,0x9a,0x65,0x4d,0xa1,0xf8}};
  void* retired = nullptr;
  if (ctx->QueryInterface(old_ext2, &retired) != E_NOINTERFACE || retired) return 8;
  MTL_TEMPORAL_UPSCALE_D3D11_DESC invalid{};
  if (ext->TemporalUpscaleEx(nullptr, MTL_FX_GENERATION_METAL4) != E_INVALIDARG ||
      ext->TemporalUpscaleEx(&invalid, MTL_FX_GENERATION_METAL3) != E_INVALIDARG ||
      ext->TemporalUpscaleEx(&invalid, static_cast<MTL_FX_GENERATION>(99)) != E_INVALIDARG)
    return 6;
  ID3D11DeviceContext *deferred = nullptr;
  check(dev->CreateDeferredContext(0, &deferred), "deferred context");
  IMTLD3D11ContextExt3 *deferred_ext = nullptr;
  check(deferred->QueryInterface(__uuidof(IMTLD3D11ContextExt3), (void**)&deferred_ext), "deferred Ext3");
  if (deferred_ext->TemporalUpscaleEx(&invalid, MTL_FX_GENERATION_METAL4) != DXGI_ERROR_INVALID_CALL)
    return 7;
  deferred_ext->Release();
  deferred->Release();
  for (auto feature : {MTL_FEATURE_METALFX_TEMPORAL_SCALER,MTL_FEATURE_METALFX_TEMPORAL_SCALER_METAL4}) {
    BOOL supported = FALSE; check(ext->CheckFeatureSupport(feature,&supported,sizeof(supported)),"feature query");
    std::printf("CAP MetalFX%d=%d\n",int(feature)+3,int(supported));
    if (!supported) return 2;
    if (ext->CheckFeatureSupport(feature,&supported,0) != E_INVALIDARG) return 3;
  }
  IDXGIDevice *gd; IDXGIAdapter *adapter; IDXGIOutput *display;
  check(dev->QueryInterface(__uuidof(IDXGIDevice),(void**)&gd),"DXGI device");
  check(gd->GetAdapter(&adapter),"adapter"); check(adapter->EnumOutputs(0,&display),"output");
  DXGI_MODE_DESC request = {}; request.Width=1920;request.Height=1080;
  request.Format=DXGI_FORMAT_R8G8B8A8_UNORM;request.RefreshRate={60000,1000};
  DXGI_MODE_DESC selected={}; check(display->FindClosestMatchingMode(&request,&selected,dev),"60 Hz closest mode");
  std::printf("DXGI closest %ux%u refresh=%u/%u\n",selected.Width,selected.Height,selected.RefreshRate.Numerator,selected.RefreshRate.Denominator);
  if (uint64_t(selected.RefreshRate.Numerator) != uint64_t(selected.RefreshRate.Denominator)*60) return 4;
  auto texture = [&](UINT size, DXGI_FORMAT format, UINT bind, const void *data, UINT pitch) {
    D3D11_TEXTURE2D_DESC d={};d.Width=d.Height=size;d.MipLevels=d.ArraySize=1;d.Format=format;
    d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=bind;
    D3D11_SUBRESOURCE_DATA init={data,pitch,0};ID3D11Texture2D *t;
    check(dev->CreateTexture2D(&d,data?&init:nullptr,&t),"texture");return t;
  };
  auto color=texture(64,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,nullptr,0);
  auto output=texture(128,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE,nullptr,0);
  std::vector<float> z(64*64,0.5f);std::vector<uint32_t> motion(128*128,0);
  auto depth=texture(64,DXGI_FORMAT_R32_FLOAT,D3D11_BIND_SHADER_RESOURCE,z.data(),64*4);
  auto mv=texture(64,DXGI_FORMAT_R16G16_FLOAT,D3D11_BIND_SHADER_RESOURCE,motion.data(),64*4);
  auto mvhi=texture(128,DXGI_FORMAT_R16G16_FLOAT,D3D11_BIND_SHADER_RESOURCE,motion.data(),128*4);
  float exp=1;auto exposure=texture(1,DXGI_FORMAT_R32_FLOAT,D3D11_BIND_SHADER_RESOURCE,&exp,4);
  D3D11_TEXTURE2D_DESC sd;output->GetDesc(&sd);sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  ID3D11Texture2D *readback;check(dev->CreateTexture2D(&sd,nullptr,&readback),"readback");
  ID3D11RenderTargetView *rtv;check(dev->CreateRenderTargetView(color,nullptr,&rtv),"RTV");
  ID3D11UnorderedAccessView *uav;check(dev->CreateUnorderedAccessView(output,nullptr,&uav),"UAV");
  for (int mode=0;mode<6;mode++) {
    for (int frame=0;frame<40;frame++) {
      float expected=(frame&1)?0.5f:0.25f;
      float c[4]={expected,0.25f,0.125f,1};float zero[4]={};
      ctx->ClearRenderTargetView(rtv,c);ctx->ClearUnorderedAccessViewFloat(uav,zero);
      MTL_TEMPORAL_UPSCALE_D3D11_DESC d={};d.InputContentWidth=d.InputContentHeight=64;
      d.AutoExposure=mode<3;d.InReset=TRUE;d.Color=color;d.Output=output;d.Depth=depth;
      d.MotionVectorInDisplayRes=mode>=3;d.MotionVector=d.MotionVectorInDisplayRes?mvhi:mv;
      d.MotionVectorScaleX=d.MotionVectorScaleY=1;d.PreExposure=1;d.ExposureTexture=d.AutoExposure?nullptr:exposure;
      if(mode%3==1) check(ext->TemporalUpscaleEx(&d, MTL_FX_GENERATION_METAL4), "temporal request acceptance");
      else if(mode%3==2) check(ext->TemporalUpscaleEx(&d, MTL_FX_GENERATION_METAL3), "temporal request acceptance");
      else ext->TemporalUpscale(&d);
      ctx->CopyResource(readback,output);
      D3D11_MAPPED_SUBRESOURCE map;check(ctx->Map(readback,0,D3D11_MAP_READ,0,&map),"Map scaler output");
      auto p=(const uint16_t*)((const char*)map.pData+64*map.RowPitch)+64*4;
      float red=half(p[0]);ctx->Unmap(readback,0);
      if(!std::isfinite(red)||std::abs(red-expected)>0.09f) {
        std::printf("FAIL mode=%d frame=%d expected=%f red=%f\n",mode,frame,expected,red);return 5;
      }
    }
    std::printf("PASS mode=%d generation=%d exposure=%s mv=%s frames=40 readback\n",mode,mode%3==1?4:3,mode<3?"auto":"texture",mode<3?"input":"display");
  }
  check(dev->GetDeviceRemovedReason(),"device status");
  std::puts("PASS MetalFX 3/4 execution, cache separation, chunk retirement and DXGI 60 Hz");
  return 0;
}
