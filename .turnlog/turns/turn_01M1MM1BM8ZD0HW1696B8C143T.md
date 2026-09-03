# Turn turn_01M1MM1BM8ZD0HW1696B8C143T

Session: sess_01M1MA74MDHYSN8MNSMVXZYNPN  
Model: unknown  
Summary: Measured live x86 D3DMetal borderless control without modifying CrossOver globals or bottle DLLs. A finalized Xcode Metal System Trace at /tmp/elite-d3dmetal-borderless-2.trace shows Elite's displayed IOSurface and Metal IOSurface access are 3024x1964 ARGB in borderless Retina mode. Temporary bottle DYLD_INSERT_LIBRARIES CAMetalLayer logger did not inject; restored Elite/cxbottle.conf byte-identically from timestamped backup. Next: capture same Metal System Trace from stock ARM64EC DXMT bottle for exact final-surface comparison.  
Created: 2026-09-03T21:50:41.800382Z

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
