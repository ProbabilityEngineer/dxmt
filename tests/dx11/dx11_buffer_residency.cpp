#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void check(HRESULT hr, const char *what) {
  if (FAILED(hr)) { std::printf("FAIL %s 0x%08lx\n",what,(unsigned long)hr);std::exit(1); }
}
int main() {
  std::setvbuf(stdout,nullptr,_IONBF,0);
  ID3D11Device *dev; ID3D11DeviceContext *ctx;
  check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,nullptr,&ctx),"device");
  char gate[16]={};GetEnvironmentVariableA("DXMT_RESIDENCY_SET",gate,sizeof(gate));
  std::printf("DXMT_RESIDENCY_SET=%s\n",gate);
  const char *shaders[]={
    "ByteAddressBuffer src:register(t0);RWByteAddressBuffer dst:register(u0);"
    "cbuffer Params:register(b0){uint delta;uint3 padding;}"
    "[numthreads(64,1,1)]void main(uint3 p:SV_DispatchThreadID){dst.Store(p.x*4,src.Load(p.x*4)+delta);}",
    "Buffer<uint> src:register(t0);RWBuffer<uint> dst:register(u0);"
    "cbuffer Params:register(b0){uint delta;uint3 padding;}"
    "[numthreads(64,1,1)]void main(uint3 p:SV_DispatchThreadID){dst[p.x]=src[p.x]+delta;}"
  };
  for (unsigned typed=0;typed<2;typed++) {
    ID3DBlob *code=nullptr,*errors=nullptr;
    HRESULT hr=D3DCompile(shaders[typed],std::strlen(shaders[typed]),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&code,&errors);
    if(FAILED(hr)&&errors)std::printf("%s\n",(char*)errors->GetBufferPointer());
    check(hr,"compile");if(errors)errors->Release();
    ID3D11ComputeShader *shader;check(dev->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader),"shader");code->Release();
    D3D11_BUFFER_DESC cd={};cd.ByteWidth=16;cd.Usage=D3D11_USAGE_DYNAMIC;cd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;cd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer *cb;check(dev->CreateBuffer(&cd,nullptr,&cb),"dynamic constant buffer");
    std::vector<ID3D11Buffer*> readbacks;
    for(unsigned batch=0;batch<40;batch++) {
      unsigned initial[64];for(unsigned i=0;i<64;i++)initial[i]=batch*1000+i;
      D3D11_BUFFER_DESC d={};d.ByteWidth=sizeof(initial);d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
      d.MiscFlags=typed?0:D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
      D3D11_SUBRESOURCE_DATA init={initial,0,0};ID3D11Buffer *buffers[2];
      check(dev->CreateBuffer(&d,&init,&buffers[0]),"input buffer");check(dev->CreateBuffer(&d,nullptr,&buffers[1]),"output buffer");
      ID3D11ShaderResourceView *srv[2];ID3D11UnorderedAccessView *uav[2];
      for(unsigned i=0;i<2;i++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd={};sd.Format=typed?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R32_TYPELESS;
        sd.ViewDimension=D3D11_SRV_DIMENSION_BUFFEREX;sd.BufferEx.NumElements=64;sd.BufferEx.Flags=typed?0:D3D11_BUFFEREX_SRV_FLAG_RAW;
        check(dev->CreateShaderResourceView(buffers[i],&sd,&srv[i]),"SRV");
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud={};ud.Format=sd.Format;ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;ud.Buffer.NumElements=64;ud.Buffer.Flags=typed?0:D3D11_BUFFER_UAV_FLAG_RAW;
        check(dev->CreateUnorderedAccessView(buffers[i],&ud,&uav[i]),"UAV");
      }
      d.BindFlags=d.MiscFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
      ID3D11Buffer *staging;check(dev->CreateBuffer(&d,nullptr,&staging),"staging");readbacks.push_back(staging);
      for(unsigned pass=0;pass<4;pass++) {
        D3D11_MAPPED_SUBRESOURCE map;check(ctx->Map(cb,0,D3D11_MAP_WRITE_DISCARD,0,&map),"rename constants");
        unsigned constants[4]={pass+1,0,0,0};std::memcpy(map.pData,constants,sizeof(constants));ctx->Unmap(cb,0);
        ctx->CSSetShader(shader,nullptr,0);ctx->CSSetConstantBuffers(0,1,&cb);
        ctx->CSSetShaderResources(0,1,&srv[pass%2]);ctx->CSSetUnorderedAccessViews(0,1,&uav[1-pass%2],nullptr);
        ctx->Dispatch(1,1,1);
        ctx->CopyResource(staging,buffers[1-pass%2]); // split compute encoders and exercise write/read hazards
        ctx->ClearState();
      }
      ctx->Flush(); // allow multiple command chunks in flight before reading any result
      for(unsigned i=0;i<2;i++){srv[i]->Release();uav[i]->Release();buffers[i]->Release();}
    }
    for(unsigned batch=0;batch<readbacks.size();batch++) {
      D3D11_MAPPED_SUBRESOURCE map;check(ctx->Map(readbacks[batch],0,D3D11_MAP_READ,0,&map),"readback");
      for(unsigned i=0;i<64;i++)if(((unsigned*)map.pData)[i]!=batch*1000+i+10){
        std::printf("FAIL typed=%u batch=%u index=%u got=%u\n",typed,batch,i,((unsigned*)map.pData)[i]);return 2;
      }
      ctx->Unmap(readbacks[batch],0);readbacks[batch]->Release();
    }
    cb->Release();shader->Release();
    std::printf("PASS %s buffers: 40 chunks, 4 passes each, renamed constants, early release and exact readback\n",typed?"typed fallback":"raw residency");
  }
  check(dev->GetDeviceRemovedReason(),"device status");ctx->Release();dev->Release();
  std::puts("PASS buffer residency lifetime and hazard regression");
}
