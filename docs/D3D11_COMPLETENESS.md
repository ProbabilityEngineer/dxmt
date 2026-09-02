# D3D11 completeness matrix

Status as of 2026-08-31. This records implemented behavior, not feature-level claims. `Partial` means DXMT may expose an interface but has documented limitations; `Unverified` means the source exists but lacks a targeted conformance result.

| Area | Status | Notes / source | Tracking |
| --- | --- | --- | --- |
| D3D11/DXBC shader conversion | Partial | SM 5.0/5.1 conversion exists. Interface-indexed resources are unsupported; some tessellation, geometry-stream, and interpolation cases remain incomplete. Exact-DXBC MSAA testing identifies an `EvaluateAttributeAtSample` mismatch (`0x3e000000` DXMT vs `0x3e200000` WARP); no renderer fix is claimed yet. | `dxm-6ex6` |
| Floating-point translation | Unverified | DXMT applies optimized fast-math flags outside `precise` paths. D3D11 arithmetic, NaN/INF, denorm, signed-zero, and contraction conformance need native-reference tests. Minimum-precision declarations are currently not applied and are not advertised through feature queries. | `dxm-6ex6` |
| Texture resource views | Partial | Main SRV/RTV/DSV/UAV path is implemented; view hazards are tracked. Unsupported view combinations must fail rather than silently translate. | `dxm-dkoy` |
| Sampler border colors | Partial | Metal exposes transparent black, opaque black, and opaque white only. DXMT rejects other valid D3D11 border colors with `E_NOTIMPL` instead of silently converting them to opaque white. | `dxm-y4j8` |
| Sampler declaration modes | Unverified | DXBC `dcl_sampler` mode is parsed but not used for linkage validation. Comparison sampler functions are mapped to Metal compare functions; arbitrary border colors remain unsupported. | `dxm-6ex6` |
| D3D11 feature queries | Partial | Feature reports require implementation evidence. ROVs, overlapping copies, SAD4 shader instructions, discard APIs, default-resource mapping, forced-sample-count RTV rendering, minimum-precision declarations, additional typed UAV-load formats, standard swizzle, and tile-based architecture reporting are reported unsupported or unknown because DXMT lacks faithful implementation/query evidence. `overlap_feature_probe` now validates the conservative `CopyWithOverlap = FALSE` report against native WARP. | `dxm-ax7i` |
| Resource hazards / UAV overlap | Implemented | Hazard tracking and UAV overlap work landed upstream in #142 and #144. This is distinct from the optional `CopyWithOverlap` feature, which remains unsupported. | upstream |
| Queries and predication | Partial | Timestamp/event/occlusion paths exist; counters and predication are not fully supported. | future ticket |
| Shared resources | Partial | D3DKMT shared resources/keyed mutex support exists; cross-process rendering is still a release-plan item. The reported shared-resource tier remains the conservative tier 0. | upstream #151 |
| Geometry stream output | Partial | Multiple stream handling is not implemented; stream-output completion is a 1.0 milestone. | upstream #151 |
| DXGI sequential swap effects | Partial | Sequential swapchain support remains a 1.0 milestone. | upstream #151 |

## Policy

1. Do not advertise an optional `D3D11_FEATURE_*` capability without an implementation reference and conformance result.
2. Do not silently substitute different D3D11 state or data semantics. Return a documented failure when no faithful Metal mapping exists.
3. Add native D3D11/reference results before changing shader translation semantics.
4. Update this matrix with every new unsupported path, conformance result, or feature-query change.
