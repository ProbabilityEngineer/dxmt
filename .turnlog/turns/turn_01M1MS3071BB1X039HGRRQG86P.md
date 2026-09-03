# Turn turn_01M1MS3071BB1X039HGRRQG86P

Session: sess_01M1MA74MDHYSN8MNSMVXZYNPN  
Model: unknown  
Summary: Published the completed HiDPI fix: moved dxmt-hidpi-swapchain bookmark to 608e41a035d0 (`dxmt: match Retina companion render targets`) and pushed it to origin. Checked ancestry: e99d9e24 DXGI EDHM compatibility commit is not an ancestor of main or dxmt-hidpi-swapchain; main also does not contain the HiDPI fix. Working copy remains in the separate source checkout with the published commit as its current change and local modifications shown by jj.  
Created: 2026-09-03T23:18:58.529596Z

## Verification

- none

## Attachments

- none

## VCS

```json
{
  "kind": "jj",
  "jj_change": "wqmqzlwqnuvs",
  "jj_commit": "608e41a035d0",
  "jj_operation": "b3a8b9955eb7",
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
