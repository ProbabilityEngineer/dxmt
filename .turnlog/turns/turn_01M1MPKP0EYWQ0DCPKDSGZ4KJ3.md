# Turn turn_01M1MPKP0EYWQ0DCPKDSGZ4KJ3

Session: sess_01M1MA74MDHYSN8MNSMVXZYNPN  
Model: unknown  
Summary: Recorded ClearView candidate validation: it changes flicker timing (faster/recover faster) but does not qualitatively eliminate flicker in Elite menus. Screenshot confirms settled 3024x1964 2x output remains sharp/full-window. ClearView partial clears are at most a secondary contributor; next work must instrument temporal D3D raster/presentation state rather than add another speculative coordinate multiplier.  
Created: 2026-09-03T22:35:39.406244Z

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
