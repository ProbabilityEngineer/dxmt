// Tests the production Metal4Temporal class with only the native bridge mocked.
#include "dxmt_metal4_temporal.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
static bool supported;
static int fail_call;
static std::atomic<int> calls, probes;
static std::array<std::atomic<int>, 8> refs;
extern "C" void NSObject_retain(obj_handle_t h) { if (h) ++refs.at(h); }
extern "C" void NSObject_release(obj_handle_t h) { if (h) CHECK(refs.at(h).fetch_sub(1) > 0); }
extern "C" bool MTLDevice_supportsFXTemporalScalerMetal4(obj_handle_t) { ++probes; return supported; }
static obj_handle_t create() {
  int call = ++calls;
  if (call == fail_call) return 0;
  obj_handle_t h = call + 1;
  CHECK(refs.at(h).fetch_add(1) == 0);
  return h;
}
extern "C" obj_handle_t MTLDevice_newMTL4Compiler(obj_handle_t) { return create(); }
extern "C" obj_handle_t MTLDevice_newMTL4CommandQueue(obj_handle_t) { return create(); }
extern "C" obj_handle_t MTLDevice_newEvent(obj_handle_t) { return create(); }
int main() {
  // Unsupported GPU, success, and failure at each of the four factories.
  for (int scenario = -1; scenario <= 4; ++scenario) {
    supported = scenario != -1;
    fail_call = scenario;
    calls = probes = 0;
    refs[1] = 1;
    {
      dxmt::Metal4Temporal state(WMT::Device(1));
      for (int i=0; i<100; ++i) CHECK(state.available() == supported);
      CHECK(probes == 1 && calls == 0); // MFX3/FSR3 capability checks stay lazy.
      std::vector<std::thread> callers;
      for (int t=0; t<16; ++t) callers.emplace_back([&] {
        for (int i=0; i<100; ++i) {
          CHECK(state.initialize() == (scenario == 0));
          if (scenario == 0) {
            CHECK(state.compiler().handle && state.queue().handle);
            CHECK(state.scene_event().handle && state.done_event().handle);
            CHECK(state.scene_event().handle != state.done_event().handle);
          }
        }
      });
      for (auto &t : callers) t.join();
      CHECK(calls == (scenario == -1 ? 0 : scenario == 1 || scenario == 2 ? 2 : 4));
      CHECK(state.available() == supported);
      if (scenario != 0) {
        CHECK(!state.compiler() && !state.queue() && !state.scene_event() && !state.done_event());
        for (size_t i=2; i<refs.size(); ++i) CHECK(refs[i] == 0);
      } else {
        CHECK(state.next_value() == 1 && state.next_value() == 2);
      }
    }
    CHECK(refs[1] == 1);
    for (size_t i=2; i<refs.size(); ++i) CHECK(refs[i] == 0);
    refs[1] = 0;
  }
  std::puts("PASS lazy Metal4: capability-only queries, 16 concurrent callers, four factory failures, distinct events, balanced lifetimes");
}
