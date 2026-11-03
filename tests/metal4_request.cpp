// Fault injection against production request preparation, with only Metal mocked.
#include "dxmt_metal4_temporal_request.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
static int calls, fail_call, begins, ends, updates, encoder_ends;
static std::array<int, 8> refs;
extern "C" void NSObject_retain(obj_handle_t h) { if (h) ++refs.at(h); }
extern "C" void NSObject_release(obj_handle_t h) { if (h) CHECK(refs.at(h)-- > 0); }
static obj_handle_t create() {
  const int call = ++calls;
  if (call == fail_call) return 0;
  const obj_handle_t h = call + 1;
  CHECK(refs.at(h)++ == 0);
  return h;
}
extern "C" obj_handle_t MTLDevice_newMTL4CommandAllocator(obj_handle_t) { return create(); }
extern "C" obj_handle_t MTLDevice_newMTL4CommandBuffer(obj_handle_t) { return create(); }
extern "C" obj_handle_t MTLDevice_newResidencySet(obj_handle_t, uint64_t capacity, obj_handle_t* error) {
  CHECK(capacity == 5);
  *error = 0;
  if (fail_call == 3) {
    // The native bridge returns a retained NSError even when the set is nil.
    CHECK(refs[5]++ == 0);
    *error = 5;
  }
  return create();
}
extern "C" void MTL4CommandBuffer_beginCommandBuffer(obj_handle_t buffer, obj_handle_t allocator) {
  CHECK(buffer && allocator); ++begins;
}
extern "C" obj_handle_t MTL4CommandBuffer_computeCommandEncoder(obj_handle_t buffer) {
  CHECK(buffer); ++calls;
  return calls == fail_call ? 0 : 7; // borrowed from the command buffer
}
extern "C" void MTL4CommandBuffer_endCommandBuffer(obj_handle_t) { ++ends; }
extern "C" void MTL4CommandEncoder_updateFence(obj_handle_t encoder, obj_handle_t fence, uint64_t stages) {
  CHECK(encoder == 7 && fence == 6 && stages == ((1ull << 27) | (1ull << 28))); ++updates;
}
extern "C" void MTL4CommandEncoder_endEncoding(obj_handle_t encoder) { CHECK(encoder == 7); ++encoder_ends; }
int main() {
  for (int scenario = 0; scenario <= 4; ++scenario) {
    calls = begins = ends = updates = encoder_ends = 0;
    fail_call = scenario;
    {
      dxmt::Metal4TemporalRequest request;
      CHECK(request.prepare(WMT::Device(1), WMT::Fence(6)) == (scenario == 0));
      CHECK(calls == (scenario >= 1 && scenario <= 3 ? 3 : 4));
      CHECK(begins == (scenario >= 1 && scenario <= 3 ? 0 : 1));
      CHECK(ends == (scenario == 4 ? 1 : 0));
      CHECK(updates == (scenario == 0 ? 1 : 0));
      CHECK(encoder_ends == updates);
      if (!scenario) {
        auto captured = request;
        CHECK(refs[2] == 2 && refs[3] == 2 && refs[4] == 2);
      }
    }
    for (auto count : refs) CHECK(count == 0);
  }
  std::puts("PASS Metal4 request: four allocation failures, no late allocation path, balanced retained objects");
}
