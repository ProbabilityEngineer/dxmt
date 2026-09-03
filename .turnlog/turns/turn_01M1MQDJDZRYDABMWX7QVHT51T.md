# Turn turn_01M1MQDJDZRYDABMWX7QVHT51T

Session: sess_01M1MA74MDHYSN8MNSMVXZYNPN  
Model: unknown  
Summary: Captured active whole-frame menu/hangar alternation trace at /tmp/elite-dxmt-menu-alternation.trace. Elite issued 512 present requests in ~8 seconds (roughly display cadence, not doubled), with only 3024x1964 output. This rules out double-present frequency and final-surface resolution as root causes. Next required diagnostic is DXMT per-frame pass/viewport telemetry to separate omitted/misrasterized menu draw passes from presentation-order failure.  
Created: 2026-09-03T22:49:47.711728Z

## Verification

- none

## Attachments

- none

## VCS

```json
{
  "kind": "jj",
  "jj_change": "wqmqzlwqnuvs",
  "jj_commit": "98f0164bfdd3",
  "jj_operation": "d4a0862510c4",
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
