# Turn turn_01M1MMNQZ71XWX8A9V0RJH39M1

Session: sess_01M1MA74MDHYSN8MNSMVXZYNPN  
Model: unknown  
Summary: Captured stock ARM64EC DXMT borderless Metal System Trace at /tmp/elite-dxmt-stock-borderless.trace. Final displayed surface is 3024x1964, but IOSurface accesses include a 1512x982 ARGB intermediate absent from D3DMetal's control trace. This directly confirms CoreAnimation upscales DXMT's half-resolution rendering surface. No source, DLL, or configuration changes made.  
Created: 2026-09-03T22:01:49.799664Z

## Verification

- none

## Attachments

- none

## VCS

```json
{
  "kind": "jj",
  "jj_change": "vkuzmuypmkuq",
  "jj_commit": "c86a6166b900",
  "jj_operation": "790fa7347f2d",
  "git_head": null,
  "git_branch": null,
  "dirty": true,
  "changed_files": [
    "src/d3d11/d3d11_swapchain.cpp",
    "src/dxmt/dxmt_presenter.cpp"
  ]
}
```
