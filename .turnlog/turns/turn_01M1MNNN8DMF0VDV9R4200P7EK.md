# Turn turn_01M1MNNN8DMF0VDV9R4200P7EK

Session: sess_01M1MA74MDHYSN8MNSMVXZYNPN  
Model: unknown  
Summary: Implemented follow-up to isolate HiDPI viewport/scissor mapping to the actual opt-in swapchain backbuffer, rather than any coincidentally 2x render target. Added resource marker set only when DXGI creates its physical backbuffer; encoder consults RTV0 marker before scaling. This targets reported flicker in Elite graphics/control menus, whose intermediate UI/composite targets must retain native D3D semantics. ARM64EC build passed (SHA-1 638b990dacc15c024a0b027b50c1e929ba79b1da) and deployed to Elite-arm-test after backing up the prior successful DLL.  
Created: 2026-09-03T22:19:15.598Z

## Verification

- none

## Attachments

- none

## VCS

```json
{
  "kind": "jj",
  "jj_change": "wqmqzlwqnuvs",
  "jj_commit": "f5f0bdf53f8c",
  "jj_operation": "19086ce0edba",
  "git_head": null,
  "git_branch": null,
  "dirty": true,
  "changed_files": [
    "src/d3d11/d3d11_context_impl.cpp",
    "src/d3d11/d3d11_resource.hpp",
    "src/d3d11/d3d11_swapchain.cpp"
  ]
}
```
