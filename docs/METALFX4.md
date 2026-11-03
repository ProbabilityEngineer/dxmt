# Experimental MetalFX 4 temporal upscaling

This local implementation ports the MetalFX changes from `661c77d` onto upstream
`a148782`. Building requires a macOS 26 SDK or newer. MetalFX 4 execution requires
macOS 26 or newer and a device accepted by `supportsMetal4FX:`.

Existing `IMTLD3D11ContextExt::TemporalUpscale` calls still use MetalFX 3. The
88-byte descriptor, existing feature value, configuration keys and spatial
upscaling path are unchanged. No presentation or refresh-rate code is changed.

A client opts into MetalFX 4 by querying `IMTLD3D11ContextExt2` and checking
`MTL_FEATURE_METALFX_TEMPORAL_SCALER_METAL4`, then calling `TemporalUpscaleEx`
with `MTL_FX_GENERATION_METAL4`. MetalFX 3 remains available through either the
legacy entry point or `TemporalUpscaleEx` with `MTL_FX_GENERATION_METAL3`.
Clients should use MetalFX 3 when the MetalFX 4 capability is unavailable.
An explicit MetalFX 4 request is never silently substituted with MetalFX 3;
unavailable capability or failed scaler creation logs an error and leaves the
output untouched. EDVR integration and in-game validation are separate work.

The Metal 4 queue is synchronized to the Metal 3 stream with two distinct events.
DXMT's resource dependency fences remain on the Metal 3 stream. Each Metal 4
encode has its own allocator and residency set, including explicit exposure
textures. These objects retire with the existing command chunk after GPU
completion. This avoids unbounded allocator growth and changing an in-flight
residency set. There are no additional CPU waits or frame-pacing mechanisms.

## Validation

`python3 tests/check_winemetal_unixtable.py` checks both Unix dispatch tables.
`tests/mtl4_temporal_abi.cpp` documents its standalone ABI compile command.
With `-Denable_tests=true`, Meson also builds `tests/dx11/dx11_metalfx.exe`.
Run that executable using the Wine runtime under test. It requires a device
supporting both generations and an output offering a 60 Hz display mode.

The GPU test checks the original MetalFX 3 entry point, explicit MetalFX 4,
then explicit MetalFX 3 in one process, with auto exposure and explicit exposure
textures, input-resolution and display-resolution motion vectors, alternating
input colors and 40 readbacks per case. It also requires DXGI to return exactly
60 Hz for a 60 Hz mode request. Run once with `MTL_DEBUG_LAYER=1` to check API
validation. These tests do not change graphics preferences or launch a game.
