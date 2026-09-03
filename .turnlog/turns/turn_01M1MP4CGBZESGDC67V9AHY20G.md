# Turn turn_01M1MP4CGBZESGDC67V9AHY20G

Session: sess_01M1MA74MDHYSN8MNSMVXZYNPN  
Model: unknown  
Summary: Investigated reported in-menu flicker after backbuffer-only viewport candidate. Live 5-second Metal trace /tmp/elite-dxmt-hidpi-menu-flicker.trace has only 3024x1964 output surfaces, ruling out half-resolution CoreAnimation scaling recurrence. Implemented next narrow candidate: scale D3D11 ClearView rectangular clear coordinates only when clearing the marked physical swapchain backbuffer, preventing logical 1512x982 partial clears from leaving stale regions in 3024x1964 UI frames. ARM64EC build succeeded, SHA-1 36806fa45ae19abe6cefc0cae2bc867836abde6e. Not deployed while game is running.  
Created: 2026-09-03T22:27:18.155071Z

## Verification

- none

## Attachments

- none

## VCS

```json
{
  "kind": "jj",
  "jj_change": "wqmqzlwqnuvs",
  "jj_commit": "da62f68cda48",
  "jj_operation": "607357b67412",
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
