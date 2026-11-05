# Private MetalFX request status contract

This is a coordinated private DXMT/EDVR change, not an upstream contribution.
It replaces the private void-returning Ext2 interface.

## Interface

`IMTLD3D11ContextExt3`, IID
`e6fc275c-395c-4134-afaf-5826ddc07e4e`, derives directly from Ext1:

```cpp
HRESULT TemporalUpscaleEx(
    const MTL_TEMPORAL_UPSCALE_D3D11_DESC* desc,
    MTL_FX_GENERATION generation);
```

The descriptor remains 88 bytes on the supported 64-bit ABIs. The explicit
generation values remain Metal3=0 and Metal4=1. The old private Ext2 IID is
not exposed. Updated EDVR requires Ext3 for **both** generations; it does not
fall back to a legacy void call or silently change generation. Upstream's
legacy Metal 3 entry point remains available for other clients.

The status-returning entry point accepts immediate contexts only.
Deferred command lists can replay, and must not share the pre-created Metal 4
command buffer/allocator between executions.

## Return semantics

- `S_OK`: the request was accepted for queued execution.
- `E_INVALIDARG`: null/missing resources, a non-2D or foreign-device resource,
  output/input alias, invalid generation/content extent or motion format.
- `DXGI_ERROR_INVALID_CALL`: a deferred context was used.
- `DXGI_ERROR_UNSUPPORTED`: the requested generation is unavailable.
- `E_FAIL`: native initialization or scaler creation failed. A nil scaler
  does not distinguish an unsupported configuration from every other native
  creation failure.
- `E_OUTOFMEMORY`: a fence or explicit Metal 4 request object could not be
  prepared.

A refused request queues no upscale. The caller must not consume its output
as a new frame. `S_OK` is **not** evidence of GPU completion or correct pixels.
GPU/device execution errors are outside this immediate return contract.

## Removing the known late refusal

Previously the encoding thread allocated the Metal 4 allocator, command
buffer and residency set. Failure logged a line and returned, after the
caller had already treated its void request as successful.

The immediate-context request now prepares these objects and the fence-host
encoder before acceptance. Failure returns to EDVR before an upscale is
queued. The retained NSError from residency-set failure is released as well.
The prepared objects travel with the queued operation. Encoding no longer
contains that explicit late allocation-and-return path.

The existing Metal 3/4 event synchronization is unchanged. The chunk retains
the allocator, command buffer and residency set until its GPU completion,
which follows the Metal 4 done-event wait. No CPU/GPU wait or readback is added
to the frame path. Preparation work now occurs on the requesting thread;
its runtime performance has not been measured.

## Validation

- `tests/metal4_request.cpp`: native Metal bridge doubles exercise each of the
  four allocation failures, retained-error cleanup and object ownership.
- `tests/metal4_lazy_init.cpp`: capability-only queries stay lazy; concurrent
  initialization, failure cleanup and distinct events remain covered.
- `tests/mtl4_temporal_abi.cpp`: descriptor layout and HRESULT method type.
- `tests/dx11/dx11_metalfx.cpp`: rejects the old IID, malformed requests and
  deferred contexts; checks request status and actual output readbacks for
  both generations, legacy Metal 3, exposure modes and motion resolutions.

The GPU test must still be run with the built runtime; compilation and
mocked tests do not establish pixel correctness. EDVR also requires its full
Windows build and resolver regression gates before deployment. No existing
bottle or installed runtime is changed by these source/build checks.

EDVR's selector limits and fail-closed source-capacity behavior are unchanged
and explicitly excluded from this work.

## Bounded memory diagnostics

After Metal 4 activation, `MetalFX-memory` info-level records are emitted
from the completion thread, after releasing a command chunk's retained
objects. Sampling is at most once every five seconds per queue, capped at
120 samples plus one suppression notice. No timer thread or GPU wait is added.
These are approximate concurrent snapshots, not a validation receipt.

- `accepted`, `submitted`, `retired`: Metal 4 request counts. Retirement means
  the owning chunk reached cleanup, not that its pixels were correct.
- `pending-encode`, `pending-retirement`: clamped differences of those counts.
- `all-chunks-*`: all command chunks, including non-MetalFX work; error chunks
  are counted separately and still retire.
- `retained-metal4-object-refs`: explicit chunk-held references to Metal 4
  allocators, command buffers and residency sets.
- `retained-resource-bindings`: four or five input/output bindings per
  submitted, unretired request. This is not a count of unique resources or
  all references held by Metal, nor a byte total.
- `scaler-cache-entries`: the immediate context's temporal scaler cache size
  at its latest accepted Metal 4 request, including both generations.
  `scalers-created` counts temporal scaler entries added over the queue lifetime.
- `metal-allocated-bytes`: the device's `currentAllocatedSize`, not process
  RSS, compressed memory, or Elite's own texture accounting.

Flat ownership counts with increasing Metal bytes point beyond the counted
chunk-held objects; rising pending counts or cache entries identify a more
specific path to inspect. Logging stops if completion itself stops, so absence
of later samples does not establish that memory stopped growing.

These records use DXMT's logger, not EDVR's `edvr_gfx_*.log`. Normally under
Wine they go to Wine's console output. `DXMT_LOG_PATH` can select a file
directory; `DXMT_LOG_LEVEL` must permit `info`. Collection settings are not
changed automatically by this diagnostic. The portable
`tests/metal4_memory_diagnostics.cpp` checks sampling bounds and concurrency,
not actual GPU retirement or memory reclamation.
