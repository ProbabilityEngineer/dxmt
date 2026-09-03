# Turn turn_01M1MRSE7CTPYHFP7HZJ8YE5MK

Session: sess_01M1MA74MDHYSN8MNSMVXZYNPN  
Model: unknown  
Summary: Validated companion-attachment fix with Elite: user reports menu alternation fixed; final capture has no `OMSetRenderTargets: invalid render targets` warnings during normal menu frames. Removed temporary frame telemetry, restored normal bottle config (only d3d11.hidpiNativeResolution=True), rebuilt final ARM64EC DLL SHA-1 58040162c13ce5778551b06279b5377db4157d44, and deployed after confirming Elite was stopped. Intended finalization: commit the documented opt-in HiDPI companion-resource fix.  
Created: 2026-09-03T23:13:45.196124Z

## Verification

- none

## Attachments

- none

## VCS

```json
{
  "kind": "jj",
  "jj_change": "wqmqzlwqnuvs",
  "jj_commit": "396444e48867",
  "jj_operation": "e3bc74224e97",
  "git_head": null,
  "git_branch": null,
  "dirty": true,
  "changed_files": [
    "dxmt.conf",
    "src/d3d11/d3d11_context_impl.cpp",
    "src/d3d11/d3d11_resource.hpp",
    "src/d3d11/d3d11_swapchain.cpp",
    "src/d3d11/d3d11_texture_device.cpp"
  ]
}
```
