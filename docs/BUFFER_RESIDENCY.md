# Experimental command-buffer buffer residency

This local change ports the narrow raw-buffer path from `8c20fc4` onto the
upstream MetalFX 3/4 build. It is enabled by default in this experimental build;
`DXMT_RESIDENCY_SET=0` restores per-encoder declarations and `=1` enables the set.
Unrecognized nonempty values disable it. The option is read once per device.
No bottle, game or EDVR setting needs to change.

Upstream already suppresses duplicate declarations within each encoder. The
older `2666a9f` experiment suppressed declarations across encoders without an
explicit command-buffer residency scope. That mechanism is not used here:
`useResource` applies to one encoder, and textures can need its other semantics.

Only raw, explicitly untracked, non-heap buffer allocations and counter buffers
enter the new path. An allocation is retained and staged once per command-buffer
sequence. The additions are batched, committed and attached before command-buffer
submission. The command chunk owns the set until GPU completion and releases
command buffers and residency sets before its retained allocations.

Textures, texture-buffer views, tracked buffers and heap allocations keep their
existing usage/stage declarations. Access tracking, fences, barriers and pass
merging are unchanged. If set creation is unavailable (including macOS before
15), the context falls back to its original declarations before omitting any.

The MetalFX 4 scaler already uses one explicit residency set for its color,
output, depth, motion and optional exposure textures. It has no repeated
`useResource` calls to remove. Its set, allocator and command-buffer lifetimes
are unchanged. This optimization reduces common D3D11 preparation work for both
MFX3 and MFX4; it does not establish the cause of their CPU/power difference.

## Targeted validation

With `enable_tests=true`, build `tests/dx11/dx11_buffer_residency.exe`. Run it
through the runtime under test with the default option and with
`DXMT_RESIDENCY_SET=0`, using `MTL_DEBUG_LAYER=1`. It checks raw SRV/UAV buffers,
typed buffer views, renamed dynamic constant buffers, four dependent compute
passes separated by copies per chunk, 40 submitted chunks per buffer type,
early resource release and exact GPU readback. These are correctness tests,
not a frame-time or power benchmark.

Also run `dx11_metalfx.exe` for both scaler generations and DXGI 60 Hz selection,
the existing seven-case texture-transition smoke test, the temporal ABI compile
check, and `tests/check_winemetal_unixtable.py`. The added Metal 3 command-buffer
bridge occupies slot 163 after the existing Metal 4 entries in both tables.
No experimental instrumentation or frame scheduling changes are included.

API reference: [Apple residency-set documentation](https://developer.apple.com/documentation/metal/mtlresidencyset).
